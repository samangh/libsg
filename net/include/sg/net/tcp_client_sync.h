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

    [[nodiscard]] std::string read_until(std::string_view delimiter);

    /**
     * @brief Reads whatever data is available, up to @p size bytes.
     *
     * @throws sg::exceptions::net::time_out if no data arrives before the timeout expires,
     *         sg::exceptions::net on other networking errors.
     * @param size maximum number of bytes to return
     */
    [[nodiscard]] std::string read_some(size_t size);

    /**
     * @brief Reads exactly @p size bytes.
     *
     * Blocks until all @p size bytes are available. Any bytes that did arrive are buffered and
     * returned by a later read, even if this call throws.
     *
     * @throws sg::exceptions::net::time_out if the bytes don't all arrive before the timeout
     *         expires, sg::exceptions::net on other networking errors.
     * @param size number of bytes to return
     */
    [[nodiscard]] std::string read(size_t size);

    void write(const std::byte* data, size_t length);
    void write(const shared_c_buffer<std::byte>& msg);
    void write(std::string_view data);

    void set_keepalive(keepalive_t);
    void set_timeout(unsigned timeoutMSec = 5000);
  private:

    boost::asio::io_context m_context;
    boost::asio::ip::tcp::socket m_socket;

    tcp_session::options_t m_options{};

    /* needed because the asio::read_until can read past the delimiter! */
    std::string m_read_leftover{};

    /* Runs the io_context until the pending operation completes, or `timeout_msec` elapses
     * (0 = wait indefinitely). Throws exceptions::net::time_out if the timeout expires. */
    void run(unsigned timeout_msec);
};

}
