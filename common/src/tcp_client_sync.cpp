#include "sg/tcp_client_sync.h"

#include "sg/string.h"

#include <boost/asio.hpp>

namespace sg::net {

tcp_client_sync::tcp_client_sync()
    : m_context(asio_io_pool::create()),
      m_socket(boost::asio::ip::tcp::socket(m_context->context())) {}
tcp_client_sync::tcp_client_sync(std::shared_ptr<asio_io_pool> context)
    : m_context(std::move(context)),
      m_socket(boost::asio::ip::tcp::socket(m_context->context())) {}
tcp_client_sync::~tcp_client_sync() { disconnect(); };

bool tcp_client_sync::is_connected() const { return (m_socket.is_open()); }
std::string tcp_client_sync::read_until(std::string_view delimiter) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    /*if previous read does not have delimiter, do new  read */
    if (auto existingPost = m_read_leftover.find(delimiter); existingPost == std::string::npos) {
        std::string result;
        boost::asio::read_until(m_socket, boost::asio::dynamic_buffer(result), delimiter);

        // add to what was leftover from previous read
        m_read_leftover += result;
    }

    // find position of string to return/keep
    auto pos = m_read_leftover.find(delimiter) + delimiter.length();

    std::string toReturn = m_read_leftover.substr(0, pos);
    m_read_leftover      = m_read_leftover.substr(pos);

    return toReturn;
}
std::string tcp_client_sync::read_some(size_t size) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    std::string toReturn;

    if (m_read_leftover.length() >= size) {
        toReturn = m_read_leftover.substr(0, size);
        m_read_leftover  = m_read_leftover.substr(size);
    } else {
        std::string result;
        boost::asio::read(m_socket,
                          boost::asio::dynamic_buffer(result, size - m_read_leftover.length()));
        m_read_leftover += result;

        toReturn = m_read_leftover.substr(0, size);
        m_read_leftover  = m_read_leftover.substr(size);
    }

    return toReturn;
}

void tcp_client_sync::connect(const end_point& endpoint) {
    if (is_connected())
        SG_THROW(std::runtime_error, "client already connected");

    boost::asio::ip::tcp::resolver resolver(m_context->context());
    auto endpoints = resolver.resolve(endpoint.ip, std::to_string(endpoint.port));
    boost::asio::connect(m_socket, endpoints);
}
void tcp_client_sync::disconnect() {
    if (m_socket.is_open()) {
        // in case the client disconnects between the above check and the next command
        try {
            m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        } catch (...) {}

        // need to close, even if the above command failed
        m_socket.close();
    }
}
void tcp_client_sync::write(const std::byte* data, size_t length) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    boost::asio::write(m_socket, boost::asio::buffer(data, length));
}
void tcp_client_sync::write(std::string_view data) {
    write(reinterpret_cast<const std::byte*>(data.data()), data.length());
}
void tcp_client_sync::write(const shared_c_buffer<std::byte>& msg) { write(msg.get(), msg.size()); }

} // namespace sg::net