#include "sg/net/net.h"

#include "sg/net/asio_io_pool.h"

#include <boost/asio.hpp>

namespace sg::net {

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
            throw exceptions::net::host_not_found(e.code().message());
        if (e.code() == boost::asio::error::network_unreachable)
            throw exceptions::net::network_unreachable(e.code().message());
        throw exceptions::net::other(e.code().message());
    }
}

} // namespace sg::net
