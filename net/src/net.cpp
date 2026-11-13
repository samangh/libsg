#include "sg/net/net.h"
#include "sg/debug.h"

#include "sg/net/asio_io_pool.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <cstddef>
#include <string>

namespace sg::net {

bool mac_address::is_zero() const noexcept {
    return *this == mac_address{};
}

std::string mac_address::to_string() const {
    constexpr char digits[] = "0123456789abcdef";

    std::string out;
    out.reserve(3 * bytes.size() - 1);
    for (auto byte : bytes) {
        if (!out.empty())
            out += ':';

        auto const value = std::to_integer<unsigned>(byte);
        out += digits[value >> 4];
        out += digits[value & 0xf];
    }

    return out;
}

std::vector<std::string> resolve(const std::string& hostname) {
    std::vector<std::string> ips;
    auto context = boost::asio::io_context();
    boost::asio::ip::tcp::resolver resolver(context);

    try {
        /* Pass a numeric service. A named one (e.g. "http") has to be looked up in /etc/services */
        for (const auto& result :
             resolver.resolve(hostname, "0", boost::asio::ip::tcp::resolver::numeric_service)) {

            /* note:
             *
             *  - the same address can come back more than once (e.g. if the address/hostname is
             *    entered multiple times in /etc/hosts)
             *  - getaddrinfo() returns IPs in order of preference, so we can't use std::set or
             *    std::unordered_set to remove duplicates: they don't keep insertion order
             */
            auto ip = result.endpoint().address().to_string();
            if (std::ranges::find(ips, ip) == ips.end())
                ips.push_back(std::move(ip));
        }

        return ips;
    } catch (const boost::system::system_error & e) {
        if (e.code() == boost::asio::error::host_not_found ||
            e.code() == boost::asio::error::host_not_found_try_again)
            SG_THROW(exceptions::net::host_not_found, e.code().message());
        if (e.code() == boost::asio::error::network_unreachable)
            SG_THROW(exceptions::net::network_unreachable, e.code().message());
        SG_THROW(exceptions::net::other, e.code().message());
    }
}

} // namespace sg::net
