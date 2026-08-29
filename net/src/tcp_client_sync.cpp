#include "sg/net/tcp_client_sync.h"

#include "sg/debug.h"

#include <chrono>
#include <thread>

namespace sg::net {

tcp_client_sync::tcp_client_sync() = default;
tcp_client_sync::~tcp_client_sync() { disconnect(); }

void tcp_client_sync::on_data(const std::byte* data, size_t size) {
    {
        std::lock_guard lock(m_mutex);
        m_read_buffer.append(reinterpret_cast<const char*>(data), size);
    }
    m_cv.notify_all();
}

void tcp_client_sync::on_disconnected(std::exception_ptr ex) {
    {
        std::lock_guard lock(m_mutex);
        m_disconnected = true;
        m_error = ex;
    }
    m_cv.notify_all();
}

void tcp_client_sync::connect(const end_point& endpoint, tcp_session::options_t options,
                              transport_factory make_transport) {
    if (is_connected())
        SG_THROW(std::runtime_error, "client already connected");

    {
        /* A fresh connection starts with a clean slate: anything left buffered from a previous one
         * must not leak into it. */
        std::lock_guard lock(m_mutex);
        m_read_buffer.clear();
        m_disconnected = false;
        m_error = nullptr;
        m_timeout_msec = options.timeout_msec;
    }

    /* Blocks until connected (and, for a negotiated transport, handshaked), throwing on failure --
     * so connect() keeps its synchronous contract. Data that arrives during connect() is buffered
     * by on_data(); connect() holds no lock while it runs, so those callbacks are free to. */
    m_client.connect(
        endpoint,
        [this](tcp_session&, const std::byte* d, size_t n) { on_data(d, n); },
        [this](tcp_session&, std::exception_ptr ex) { on_disconnected(ex); }, options,
        std::move(make_transport));
}

void tcp_client_sync::disconnect() noexcept {
    /* noexcept: the destructor calls this. tcp_client::disconnect() waits for the session to stop,
     * so once it returns no more callbacks can fire. */
    try {
        m_client.disconnect();
    } catch (...) {
    }
}

bool tcp_client_sync::is_connected() const { return m_client.is_connected(); }

void tcp_client_sync::throw_disconnect_error() {
    /* Translate exceptions */
    if (m_error) {
        try {
            std::rethrow_exception(m_error);
        } catch (const exceptions::net::time_out&) {
            throw;
        } catch (const std::exception& e) {
            SG_THROW(exceptions::net::other, e.what());
        } catch (...) {
            SG_THROW(exceptions::net::other, "the connection was closed");
        }
    }
    SG_THROW(exceptions::net::other, "the connection was closed");
}

template <typename Ready>
void tcp_client_sync::wait_for(std::unique_lock<std::mutex>& lock, Ready ready) {
    /* Nothing more can arrive on a session that is already down; give whatever is buffered a last
     * chance through `ready()`, otherwise report why it went (or that we were never connected). */
    if (!ready() && !m_client.is_connected()) {
        if (m_disconnected)
            throw_disconnect_error();
        SG_THROW(std::runtime_error, "client not connected");
    }

    const bool has_timeout = m_timeout_msec != 0;
    const auto pred = [&] { return ready() || m_disconnected; };

    if (has_timeout) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(m_timeout_msec);
        if (!m_cv.wait_until(lock, deadline, pred))
            SG_THROW(exceptions::net::time_out); // timed out; whatever arrived stays buffered
    } else {
        m_cv.wait(lock, pred);
    }

    /* pred holds: either the data we wanted is here, or the peer went. Serve the data if we have it,
     * even on a disconnect; only report the disconnect once there is nothing left to give. */
    if (!ready())
        throw_disconnect_error();
}

std::string tcp_client_sync::read_some() {
    std::unique_lock lock(m_mutex);
    if (m_read_buffer.empty())
        wait_for(lock, [this] { return !m_read_buffer.empty(); });

    std::string result;
    result.swap(m_read_buffer);
    return result;
}

std::string tcp_client_sync::read(size_t size) {
    std::unique_lock lock(m_mutex);
    if (m_read_buffer.size() < size)
        wait_for(lock, [this, size] { return m_read_buffer.size() >= size; });

    std::string result = m_read_buffer.substr(0, size);
    m_read_buffer.erase(0, size);
    return result;
}

std::string tcp_client_sync::read_until(std::string_view delimiter) {
    std::unique_lock lock(m_mutex);

    size_t pos = std::string::npos;
    auto found = [&] { return (pos = m_read_buffer.find(delimiter)) != std::string::npos; };

    if (!found())
        wait_for(lock, found);

    const size_t n = pos + delimiter.size();
    std::string result = m_read_buffer.substr(0, n);
    m_read_buffer.erase(0, n);
    return result;
}

void tcp_client_sync::write(const std::byte* data, size_t length) {
    if (!is_connected())
        SG_THROW(std::runtime_error, "client not connected");

    auto& session = m_client.session();
    session.write(data, length);

    /* You could block here until, for example,session.pending_bytes() reads zero. But there is no
     * point as the OS caches the writes anyway */
}

void tcp_client_sync::write(std::string_view data) {
    write(reinterpret_cast<const std::byte*>(data.data()), data.length());
}

void tcp_client_sync::write(const shared_c_buffer<std::byte>& msg) { write(msg.get(), msg.size()); }

void tcp_client_sync::set_keepalive(keepalive_t keepAlive) {
    if (is_connected())
        m_client.session().set_keepalive(keepAlive);
}

void tcp_client_sync::set_timeout(unsigned timeoutMSec) {
    {
        std::lock_guard lock(m_mutex);
        m_timeout_msec = timeoutMSec;
    }
    if (is_connected())
        m_client.session().set_timeout(timeoutMSec);
}

} // namespace sg::net
