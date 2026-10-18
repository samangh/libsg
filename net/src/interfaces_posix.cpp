#include "include/interfaces_common.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <ifaddrs.h>
#include <map>
#include <memory>
#include <net/if.h>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
    #error "This file should only be used in POSIX systems"
#endif

#if defined(__linux__)
    #include <netpacket/packet.h>
#elif !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(__GNU__)
    /* Cygwin, MSYS and GNU/Hurd have no AF_LINK, sockaddr_dl or LLADDR */
    #include <net/if_dl.h>
#endif

/* macOS and other BSD-derived systems carry a length in every sockaddr, and use it */
#if defined(__APPLE__)
    #define SG_SOCKADDR_HAS_SA_LEN 1
#endif

namespace {

/* Hardware addresses arrive as their own AF_PACKET (Linux) / AF_LINK (BSD, macOS) entries rather
 * than attached to the IP entries, so they have to be collected and joined by interface name.
 *
 * Returns nothing when this entry is not a link-layer record at all, as opposed to an interface
 * whose hardware address is absent -- that comes back as an all-zero address. */
std::optional<mac_address> hardware_address(const ifaddrs *ifa) {
    if (ifa->ifa_addr == nullptr)
        return std::nullopt;

#if defined(__linux__)
    if (ifa->ifa_addr->sa_family != AF_PACKET)
        return std::nullopt;

    auto const *ll = reinterpret_cast<const sockaddr_ll *>(ifa->ifa_addr);
    return to_mac(ll->sll_addr, ll->sll_halen);
#elif defined(__CYGWIN__) || defined(__MSYS__) || defined(__GNU__)
    return std::nullopt; /* no link-layer sockaddr to read */
#else
    if (ifa->ifa_addr->sa_family != AF_LINK)
        return std::nullopt;

    auto const *dl = reinterpret_cast<const sockaddr_dl *>(ifa->ifa_addr);
    return to_mac(LLADDR(dl), dl->sdl_alen);
#endif
}

/* Copies a netmask into a fixed buffer, zero-filling whatever the OS left off.
 *
 * Note that the family comes from the address the mask belongs to, never from the mask itself:
 * BSD-derived systems, macOS included, frequently leave the mask's own sa_family unset. */
std::array<std::byte, 16> netmask_bytes(const sockaddr *netmask, address_family family) {
    auto const offset = address_offset(family);
    size_t available = address_size(family);

#ifdef SG_SOCKADDR_HAS_SA_LEN
    /* BSD kernels compact route masks by dropping trailing zero bytes, so a /64 can arrive in
     * fewer bytes than sizeof(sockaddr_in6); reading a fixed 16 would run off the record. */
    available =
        netmask->sa_len > offset ? std::min<size_t>(netmask->sa_len - offset, available) : 0;
#endif

    std::array<std::byte, 16> bytes{};
    std::memcpy(bytes.data(), reinterpret_cast<const std::byte *>(netmask) + offset, available);

    return bytes;
}

} // namespace

namespace sg::net {

std::vector<interface_details> interfaces() {
    ifaddrs *head = nullptr;
    if (::getifaddrs(&head) != 0)
        throw sg::exceptions::net::other(std::system_category().message(errno));

    const std::unique_ptr<ifaddrs, decltype(&::freeifaddrs)> list{head, &::freeifaddrs};

    std::map<std::string, mac_address> macs;
    for (auto const *ifa = list.get(); ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name == nullptr)
            continue;

        if (auto mac = hardware_address(ifa))
            macs.insert_or_assign(ifa->ifa_name, *mac);
    }

    std::vector<interface_details> result;
    for (auto const *ifa = list.get(); ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)
            continue;

        /* only report interfaces that are usable right now */
        if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_RUNNING) == 0)
            continue;

        auto const family = to_family(ifa->ifa_addr->sa_family);
        auto const scope_id = scope_id_of(ifa->ifa_addr);

        netmask_t mask;
        if (ifa->ifa_netmask != nullptr)
            mask = to_netmask(family, netmask_bytes(ifa->ifa_netmask, family));

        /* all-zero when this interface has no hardware address at all */
        mac_address mac;
        if (auto const found = macs.find(ifa->ifa_name); found != macs.end())
            mac = found->second;

        result.push_back({.name = ifa->ifa_name,
                          .family = family,
                          .address = address_to_string(ifa->ifa_addr, family, scope_id),
                          .netmask = std::move(mask.text),
                          .prefix_length = mask.prefix_length,
                          .scope_id = scope_id,
                          .physical_address = mac,
                          .is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0});
    }

    return result;
}

} // namespace sg::net
