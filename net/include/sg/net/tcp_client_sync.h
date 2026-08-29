#pragma once

#include "net.h"
#include "tcp_client.h"
#include "tcp_session.h"
#include "tcp_transport.h"

#include <sg/buffer.h>
#include <sg/export/net.h>

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>

namespace sg::net {

/** A blocking TCP client.
 *
 * Note that this client will spawn a thread.
 *
 * This is a wrapper around tcp_client(). The last commit that had a different implemenation of
 * this is a0c2fad.
 */
class SG_NET_EXPORT tcp_client_sync {
  public:
    tcp_client_sync();
    virtual ~tcp_client_sync();

    void connect(const end_point& endpoint, tcp_session::options_t options = {},
                 transport_factory make_transport = {});
    void disconnect() noexcept;

    [[nodiscard]] bool is_connected() const;

    /**
     * @brief Reads exactly @p size bytes.
     *
     * Blocks until all @p size bytes are available. Any bytes that did arrive are buffered and
     * returned by a later read, even if this call throws.
     *
     * @throws sg::exceptions::net::time_out if the bytes don't all arrive before the timeout
     *         expires, sg::exceptions::net::other on other networking errors.
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
    /* Guards the received-data buffer and the disconnect state */
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    std::string m_read_buffer;
    unsigned m_timeout_msec{5000};

    bool m_disconnected{false};
    std::exception_ptr m_error;   // why the session stopped, if not cleanly

    /* Declared last so it is destroyed first: the session (and thus its callbacks) is torn down
     * before the state above goes away. */
    tcp_client m_client;

    void on_data(const std::byte* data, size_t size);
    void on_disconnected(std::exception_ptr ex);

    [[noreturn]] void throw_disconnect_error();

    /* Waits until a predicate returns true. Uses lock/conditional-variable */
    template <typename Ready>
    void wait_for(std::unique_lock<std::mutex>& lock, Ready ready);
};

} // namespace sg::net
