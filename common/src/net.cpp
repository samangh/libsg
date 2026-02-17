#include "sg/net.h"
#include "sg/libuv_wrapper.h"
#include "sg/tcp_context.h"

#include <boost/asio.hpp>
#include <vector>

namespace sg::net {

std::vector<sg::net::interface_details> interfaces()
{
    uv_interface_address_t *uv_interface=nullptr;
    int count;

    /* get interfaces and populate buffer */
    THROW_ON_LIBUV_ERROR(uv_interface_addresses(&uv_interface, &count));

    /* buffer for storing data, the longest possible is an IPv6 so use that */
    char ip_str[INET6_ADDRSTRLEN];

    try {
        std::vector<sg::net::interface_details> vec;
        for (int i = 0; i < count; ++i) {
            sg::net::interface_details int_det;
            std::copy(std::begin(uv_interface[i].phys_addr), std::end(uv_interface[i].phys_addr), std::begin(int_det.physical_address));

            THROW_ON_LIBUV_ERROR(
                uv_ip_name((struct sockaddr *)&uv_interface[i].address, ip_str, sizeof(ip_str)));
            int_det.address = ip_str;

            THROW_ON_LIBUV_ERROR(
                uv_ip_name((struct sockaddr *)&uv_interface[i].netmask, ip_str, sizeof(ip_str)));
            int_det.netmask = ip_str;

            int_det.is_internal = uv_interface[i].is_internal;

            if (int_det.address.find(".") != int_det.address.npos)
                int_det.family = sg::net::address_family::IPv4;
            else
                int_det.family = sg::net::address_family::IPv6;

            vec.push_back(int_det);
        }

        uv_free_interface_addresses(uv_interface, count);
        return vec;
    } catch (...) {
        /* ensure buffer is free even if there is an error */
        uv_free_interface_addresses(uv_interface, count);
        throw;
    }

}
std::vector<std::string> resolve(const std::string& hostname) {
    std::vector<std::string> ips;
    auto context = boost::asio::io_context();
    boost::asio::ip::tcp::resolver resolver(context);

    try {
        for (const auto& result : resolver.resolve(hostname, "http"))
            ips.push_back(result.endpoint().address().to_string());

        return ips;
    } catch (const boost::system::system_error & e) {
        if (e.code() == boost::asio::error::host_not_found ||
            e.code() == boost::asio::error::host_not_found_try_again)
            throw exceptions::net<exceptions::errors::net::host_not_found>(e.code().message());
        if (e.code() == boost::asio::error::network_unreachable)
            throw exceptions::net<exceptions::errors::net::network_unreachable>(e.code().message());
        throw exceptions::net<exceptions::errors::net::other>(e.code().message());
    }
}

} // namespace sg::net
