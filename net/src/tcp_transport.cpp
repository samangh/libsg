#include "sg/net/tcp_transport.h"
#include "sg/net/tcp_transport_plain.h"
#include "transport_timeout.h"

namespace sg::net {

std::unique_ptr<tcp_transport> make_transport(const transport_factory& factory,
                                          boost::asio::ip::tcp::socket& socket) {
    if (factory)
        return factory(socket);

    return std::make_unique<tcp_transport_plain>(socket);
}

} // namespace sg::net
