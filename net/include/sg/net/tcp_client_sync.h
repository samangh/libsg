#pragma once

#include "asio_io_pool.h"
#include "net.h"
#include "tcp_session.h"
#include <sg/buffer.h>
#include <sg/debug.h>

#include <boost/asio/ip/tcp.hpp>
#include <sg/export/net.h>

namespace sg::net {

class SG_NET_EXPORT tcp_client_sync {
  public:
    tcp_client_sync();
    virtual ~tcp_client_sync();

    void connect(const end_point& endpoint, tcp_session::options_t options = {});
    void disconnect();

    [[nodiscard]] bool is_connected() const;

    /**
     * @brief Reads exactly @p size bytes.
     *
     * Blocks until all @p size bytes are available. Any bytes that did arrive are buffered and
     * returned by a later read, even if this call throws.
     *
     * @throws sg::exceptions::net::time_out if the bytes don't all arrive before the timeout
     *         expires, sg::exceptions::net::other on other networking errors.
     * @param size number of bytes to return
     */
    [[nodiscard]] std::string read(size_t size);

    /**
     * @brief Reads up to, and including, the first @p delimiter.
     *
     * Any bytes read past the delimiter, or that arrived before this call timed out, are buffered
     * and returned by a later read.
     *
     * @throws sg::exceptions::net::time_out if no delimiter arrives before the timeout expires,
     *         sg::exceptions::net::other on other networking errors.
     */
    [[nodiscard]] std::string read_until(std::string_view delimiter);

    /**
     * @brief Reads whatever data is available.
     *
     * @throws sg::exceptions::net::time_out if no data arrives before the timeout expires,
     *         sg::exceptions::net::other on other networking errors.
     */
    [[nodiscard]] std::string read_some();

    void write(const std::byte* data, size_t length);
    void write(const shared_c_buffer<std::byte>& msg);
    void write(std::string_view data);

    void set_keepalive(keepalive_t);
    void set_timeout(unsigned timeoutMSec = 5000);
  private:
    boost::asio::io_context m_context;
    boost::asio::ip::tcp::socket m_socket;

    /* holds data that has been read from the socket but not yet returned to the caller: bytes
     * read past a delimiter, and bytes that arrived before a read timed out */
    std::string m_read_buffer;

    /* temp buffer, placed here so that we don't continuously re-create this */
    std::array<char, 64*1024> m_read_some_buf;

    tcp_session::options_t m_options{};

    /* Throws for the error code of a failed read. A timed out read keeps the connection, as
     * whatever did arrive is buffered and can be returned by a later read. */
    void throw_on_read_error(const boost::system::error_code& ec);
};

}
