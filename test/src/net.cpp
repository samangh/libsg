#include <catch2/catch_test_macros.hpp>
#include <sg/net/net.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>

/* Parses a dotted-quad netmask into a host-order integer. Deliberately an independent
 * implementation of what the library does, so that it can disagree. */
static uint32_t parse_ipv4_netmask(const std::string& mask) {
    unsigned a{}, b{}, c{}, d{};
    REQUIRE(std::sscanf(mask.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4);
    REQUIRE((a <= 255 && b <= 255 && c <= 255 && d <= 255));
    return (a << 24) | (b << 16) | (c << 8) | d;
}

TEST_CASE("sg::net check interfaces() returns a loopback address", "[sg::net]") {
    auto loopback =
        sg::net::interfaces() | std::views::filter([](const sg::net::interface_details& inter) {
            return (inter.address == "127.0.0.1" && inter.is_loopback &&
                    inter.family == sg::net::address_family::IPv4) ||
                   (inter.address == "::1" && inter.is_loopback &&
                    inter.family == sg::net::address_family::IPv6);
        });
    REQUIRE(!std::ranges::empty(loopback));
}

TEST_CASE("sg::net interfaces() populates every entry", "[sg::net]") {
    auto const ifaces = sg::net::interfaces();
    REQUIRE(!ifaces.empty());

    for (const auto& inter : ifaces) {
        CAPTURE(inter.name, inter.address, inter.netmask, inter.prefix_length);

        REQUIRE(!inter.name.empty());
        REQUIRE(!inter.address.empty());

        auto const max_prefix = inter.family == sg::net::address_family::IPv4 ? 32u : 128u;
        REQUIRE(inter.prefix_length <= max_prefix);

        /* A reported netmask always has at least one bit set -- no real interface is a /0. A
         * zero prefix next to a non-empty netmask means the mask was read wrongly, which an
         * agreement check cannot see because an all-zero mask agrees with a zero prefix. */
        if (!inter.netmask.empty())
            REQUIRE(inter.prefix_length > 0);

        /* an IPv4 address never renders as IPv6, or vice versa */
        if (inter.family == sg::net::address_family::IPv4) {
            REQUIRE(inter.address.find(':') == std::string::npos);
            REQUIRE(inter.netmask.find(':') == std::string::npos);
            REQUIRE(inter.scope_id == 0);
        } else {
            REQUIRE(inter.address.find(':') != std::string::npos);
        }

        /* a zone suffix appears if and only if there is a zone to report */
        if (inter.scope_id != 0)
            REQUIRE(inter.address.ends_with("%" + std::to_string(inter.scope_id)));
        else
            REQUIRE(inter.address.find('%') == std::string::npos);
    }
}

/* Expands an IPv6 netmask into its eight 16-bit groups. A netmask's zero bits are always a
 * trailing run, so inet_ntop only ever compresses the tail, which keeps this simple. */
static std::array<uint16_t, 8> parse_ipv6_netmask(const std::string& mask) {
    auto body = mask;
    if (body.ends_with("::"))
        body.resize(body.size() - 2);

    std::array<uint16_t, 8> groups{};
    size_t index = 0;
    for (size_t pos = 0; !body.empty();) {
        auto const colon = body.find(':', pos);
        auto const len = (colon == std::string::npos ? body.size() : colon) - pos;
        REQUIRE(len > 0);
        REQUIRE(index < groups.size());
        groups[index++] = static_cast<uint16_t>(std::stoul(body.substr(pos, len), nullptr, 16));
        if (colon == std::string::npos)
            break;
        pos = colon + 1;
    }

    return groups;
}

TEST_CASE("sg::net interfaces() netmask and prefix_length agree", "[sg::net]") {
    for (const auto& inter : sg::net::interfaces()) {
        if (inter.family != sg::net::address_family::IPv4 || inter.netmask.empty())
            continue;

        CAPTURE(inter.name, inter.netmask, inter.prefix_length);
        auto const mask = parse_ipv4_netmask(inter.netmask);

        /* the reported prefix really is this mask's bit count */
        REQUIRE(static_cast<unsigned>(std::popcount(mask)) == inter.prefix_length);

        /* a netmask's set bits run from the top with no gaps. A truncated or over-read mask
         * shows up here as stray low bits, which a bounds check on prefix_length would miss. */
        if (mask != 0)
            REQUIRE((mask | (mask - 1)) == 0xFFFFFFFFu);
    }
}

TEST_CASE("sg::net interfaces() IPv6 netmask and prefix_length agree", "[sg::net]") {
    for (const auto& inter : sg::net::interfaces()) {
        if (inter.family != sg::net::address_family::IPv6 || inter.netmask.empty())
            continue;

        CAPTURE(inter.name, inter.netmask, inter.prefix_length);
        auto const groups = parse_ipv6_netmask(inter.netmask);

        unsigned bits = 0;
        uint16_t previous = 0xffff;
        for (auto group : groups) {
            CAPTURE(group);

            /* groups never increase, so the zeros stay at the bottom */
            REQUIRE(group <= previous);

            /* and each group is itself a run of ones from the top. Garbage read past the end of
             * a truncated netmask lands here, as a group that is not a legal mask at all. */
            auto const count = static_cast<unsigned>(std::popcount(group));
            REQUIRE(group == static_cast<uint16_t>(0xFFFFu << (16 - count)));

            bits += count;
            previous = group;
        }

        REQUIRE(bits == inter.prefix_length);
    }
}

TEST_CASE("sg::net interfaces() reports the loopback netmask and prefix", "[sg::net]") {
    auto found = false;

    for (const auto& inter : sg::net::interfaces()) {
        if (!inter.is_loopback)
            continue;

        /* loopback never has a real hardware address */
        REQUIRE(inter.physical_address.is_zero());

        if (inter.address == "127.0.0.1") {
            REQUIRE(inter.prefix_length == 8);
            REQUIRE(inter.netmask == "255.0.0.0");
            found = true;
        } else if (inter.address == "::1") {
            REQUIRE(inter.prefix_length == 128);
            REQUIRE(inter.netmask == "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
            found = true;
        }
    }

    REQUIRE(found);
}

TEST_CASE("sg::net interfaces() joins hardware addresses to their interface", "[sg::net]") {
    auto const ifaces = sg::net::interfaces();

    /* Every address of one interface reports that interface's name and hardware address. This is
     * the promise the docs make, and on POSIX the MAC is collected from separate link-layer
     * entries and joined by name, so it is worth checking it actually lands. */
    std::map<std::string, sg::net::mac_address> by_name;
    for (const auto& inter : ifaces) {
        auto const [it, inserted] = by_name.emplace(inter.name, inter.physical_address);
        if (!inserted) {
            CAPTURE(inter.name);
            REQUIRE(it->second == inter.physical_address);
        }
    }

    /* A host with a real NIC must produce at least one non-zero MAC, otherwise the join is
     * silently broken. Skipped on a loopback-only host, where there is nothing to join. */
    auto const has_real_nic =
        std::ranges::any_of(ifaces, [](const auto& i) { return !i.is_loopback; });
    if (has_real_nic)
        REQUIRE(std::ranges::any_of(ifaces, [](const auto& i) {
            return !i.is_loopback && !i.physical_address.is_zero();
        }));
}

TEST_CASE("sg::net mac_address formatting", "[sg::net]") {
    REQUIRE(sg::net::mac_address{}.is_zero());
    REQUIRE(sg::net::mac_address{}.to_string() == "00:00:00:00:00:00");

    sg::net::mac_address const mac{{std::byte{0x02}, std::byte{0x42}, std::byte{0xac},
                                    std::byte{0x11}, std::byte{0x00}, std::byte{0xfe}}};
    REQUIRE(!mac.is_zero());
    REQUIRE(mac.to_string() == "02:42:ac:11:00:fe");
    REQUIRE(mac != sg::net::mac_address{});
}

TEST_CASE("sg::net check resolve()", "[sg::net]") {
    // check loopback
    auto loopback = sg::net::resolve("localhost");
    REQUIRE((!loopback.empty() && (sg::net::resolve("localhost").front() == "127.0.0.1" ||
                                   sg::net::resolve("localhost").front() == "::1")));

    // check exact error
    REQUIRE_THROWS_AS(sg::net::resolve("localhozxcst"),
                      sg::exceptions::net::host_not_found);

    // Check higher-level error
    try {
        sg::net::resolve("localhozxcst");
    } catch (const sg::exceptions::net::any&) {}
}