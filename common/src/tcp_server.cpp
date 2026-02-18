#include "sg/tcp_server.h"
#include "sg/tcp_native.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <atomic>

namespace sg::net {

tcp_server::~tcp_server() noexcept(false) {
    if (m_context && m_context->is_running()) {
        stop_async();
        m_context->wait_for_stop();
    }

    m_pool.wait_for_tasks();
}

void tcp_server::stop_async() {
    /* if stop thread not started, start */
    if (m_stop_in_operation.exchange(true))
        return;

    m_stopping_thread = std::jthread([this]() {
        for (auto& acceptor : m_acceptors) {
            try {
                acceptor->close();
            } catch (...) {}
        }

        /* wait until all acceptors have disconnected */
        m_acceptors_stopped.wait(false, std::memory_order::acquire);
        disconnect_all();

        /* wait until all clients disconnected and all callbacks called, etc */
        while (clients_count() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (m_context->is_running())
            m_context->stop_async();
    });
}

void tcp_server::start(std::vector<end_point> endpoints, started_listening_cb_t onStartListening,
                       stopped_listening_cb_t onStopListeniing, session_created_cb_t onNewSession,
                       session_data_available_cb_t onDataAvailCb,
                       session_disconnected_cb_t onDisconnCb, size_t noThreads) noexcept(false) {
    if (m_context && m_context->is_running())
        throw std::runtime_error("tcp_server is already running");

    auto stoppedTask = std::bind(&tcp_server::on_worker_stop, this, std::placeholders::_1);
    m_context = tcp_context::create(noThreads, stoppedTask);

    m_on_started_listening_cb = onStartListening;
    m_on_stopped_listening_cb = onStopListeniing;
    m_new_session_cb = std::move(onNewSession), m_on_data_read_user_cb = std::move(onDataAvailCb);
    m_on_disconnect_user_cb = std::move(onDisconnCb);

    m_stop_in_operation.store(false);

    m_endpoints = endpoints;
    m_last_id = 0;
    m_acceptors.clear();

    m_promise_started_listening = std::promise<void>();

    on_worker_start();
    m_context->run();
}

void tcp_server::future_get_once() noexcept(false) { m_context->future_get_once(); }

bool tcp_server::is_stopped() const {
    if (m_context)
        return !(m_context->is_running());

    return true;
}

size_t tcp_server::clients_count() const {
    std::shared_lock lock(m_mutex);
    return m_sessions.size();
}

void tcp_server::write(session_id_t id, const void* data, size_t size) {
    if (m_stop_in_operation.load(std::memory_order::acquire))
        throw std::runtime_error("can't write as a stop has been requested");

    auto ptr = sg::make_shared_c_buffer<std::byte>(size);
    std::memcpy(ptr.get(), data, size);
    write(id, ptr);
}

void tcp_server::write(session_id_t id, sg::shared_c_buffer<std::byte> buffer) {
    std::shared_lock lock(m_mutex);
    m_sessions.at(id)->write(buffer);
}

void tcp_server::disconnect(session_id_t id) {
    std::shared_lock lock(m_mutex);
    m_sessions.at(id)->stop_async();
}

void tcp_server::disconnect_all() {
    std::shared_lock lock(m_mutex);
    for (auto& [_, sess] : m_sessions) sess->stop_async();
}

tcp_session* tcp_server::session(session_id_t id) {
    std::shared_lock lock(m_mutex);
    return m_sessions.at(id).get();
}
void tcp_server::set_keepalive(bool enableKeepAlive, unsigned idleSec, unsigned intervalSec,
                               unsigned count) {
    for (auto a: m_acceptors) {
        sg::net::native::set_keepalive(a->native_handle(), enableKeepAlive, idleSec, intervalSec, count);
    }

}
void tcp_server::set_timeout(unsigned timeoutMSec) {
    for (auto a: m_acceptors)
        sg::net::native::set_timeout(a->native_handle(), timeoutMSec);
}

boost::asio::awaitable<void>
tcp_server::listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor) {
    ++m_acceptors_running_count;
    m_acceptors_stopped.store(false, std::memory_order::release);

    try {
        while (!m_stop_in_operation.load(std::memory_order::acquire)) {
            auto id = m_last_id++;

            auto onSessionDisconnected = [this, id](std::optional<std::exception> ex) {
                on_session_stopped(id, ex);
            };
            auto onData = [this, id](const std::byte* data, size_t size) {
                inform_user_of_data(id, data, size);
            };

            auto sess = std::make_unique<tcp_session>(
                co_await acceptor.get()->async_accept(boost::asio::use_awaitable),
                onData,
                onSessionDisconnected);

            /* check that the m_async did not return because stop_async was called */
            if (!m_stop_in_operation.load(std::memory_order::acquire)) {
                {
                    std::unique_lock lock(m_mutex);
                    m_sessions.emplace(id, std::move(sess));
                }

                if (m_new_session_cb)
                    m_new_session_cb(*this, id);

                session(id)->start();
            }
        }
    } catch (const boost::system::system_error& err) {
        //TODO: how to handle exceptions?

        // this is the error that you get when the socket is closed
        // err.code() == boost::asio::error::operation_aborted)
    } catch (...) {}

    if (--m_acceptors_running_count==0) {
        m_acceptors_stopped.store(true);
        m_acceptors_stopped.notify_all();
    }
}

void tcp_server::on_worker_start() {
    for (auto e : m_endpoints) {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip), e.port);

        /** The boost acceptor(conext, endpoint) function is equivalent to:
         *
         * @code
         * basic_socket_acceptor<Protocol> acceptor(my_context);
         * acceptor.open(endpoint.protocol());
         * if (reuse_addr)
         *   acceptor.set_option(socket_base::reuse_address(true));
         * acceptor.bind(endpoint);
         * acceptor.listen();
         * @endcode
         *
         * So may be we should do above directly (in case we want to add our own options). */
        auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(m_context->context(), ep);
        boost::asio::co_spawn(m_context->context(), listener(a), boost::asio::detached);
        m_acceptors.push_back(a);
    }

    if (m_on_started_listening_cb)
        m_on_started_listening_cb(*this);
}

void tcp_server::on_worker_stop(tcp_context&) {
    if (m_on_stopped_listening_cb)
        m_on_stopped_listening_cb(*this);
}


void tcp_server::inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
    if (m_on_data_read_user_cb)
        m_on_data_read_user_cb(*this, id, data, size);
}

void tcp_server::on_session_stopped(session_id_t id, std::optional<std::exception> ex) {
    /* This needs to be on a separate thread-loop, to get out of the following deadlock:
     *
     *   1) client disconnects so the function gets called (via the on_disconnection callback)
     *   2) this function removes the session, which calls the destructor of the session
     *   3) the destructor waits for the session to end, but that can't happen because the
     *      on_disconnection callback is running
     */
    m_pool.enqueue_detach([this, id, ex]() {
        if (m_on_disconnect_user_cb)
            m_on_disconnect_user_cb(*this, id, ex);

        std::unique_lock lock(m_mutex);
        m_sessions.erase(id);
    });
}
}
