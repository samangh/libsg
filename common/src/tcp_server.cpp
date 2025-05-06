#include "sg/tcp_server.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace sg::net {

void tcp_server::stop_async() {
    /* if stop thread not started, start */
    if (m_stop_in_operation.exchange(true))
        return;

    m_stopping_thread = std::jthread([this]() {
        /* stop accepting new connections */
        {
            std::lock_guard lock(m_mutex);
            for (auto& acceptor : m_acceptors)
                if (acceptor->is_open())
                    acceptor->close();
        }

        disconnect_all();

        /* wait untill all clients disconnected and all callbacks called, etc */
        while (clients_count() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

        on_stop();
    });
}

tcp_server::tcp_server() : m_context(tcp_context::create()) {}

tcp_server::tcp_server(std::shared_ptr<tcp_context> context) : m_context(context) {}

tcp_server::~tcp_server() {
    if (!m_is_running)
        return;

    stop_async();
    wait_until_stopped();
}

void tcp_server::start(std::vector<end_point> endpoints,
                       started_listening_cb_t onStartListening,
                       stopped_listening_cb_t onStopListeniing,
                       session_created_cb_t onNewSession,
                       session_data_available_cb_t onDataAvailCb,
                       session_disconnected_cb_t onDisconnCb) noexcept(false) {
    if (m_is_running.exchange(true))
        throw std::runtime_error("this tcp_server is already running");

    try {
        m_on_started_listening_cb = onStartListening;
        m_on_stopped_listening_cb = onStopListeniing;
        m_new_session_cb = std::move(onNewSession),
        m_on_data_read_user_cb = std::move(onDataAvailCb);
        m_on_disconnect_user_cb = std::move(onDisconnCb);

        m_stop_in_operation.store(false);

        m_endpoints = endpoints;
        m_acceptors.clear();
        m_last_id = 0;

        m_promise_started_listening = std::promise<void>();

        listen_on_endpoints();
        m_context->run();
    } catch (...) {
        m_is_running.exchange(false);
        m_is_running.notify_all();
        throw;
    }

    if (m_on_started_listening_cb)
        m_on_started_listening_cb(*this);
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
    for (auto [_, sess] : m_sessions) sess->stop_async();
}

tcp_server::ptr tcp_server::session(session_id_t id) {
    std::shared_lock lock(m_mutex);
    return m_sessions.at(id);
}

void tcp_server::wait_until_stopped() const {
    /* work around spuris wakeups */
    while(m_is_running.load(std::memory_order::acquire))
        m_is_running.wait(true);
}

boost::asio::awaitable<void> tcp_server::listener(boost::asio::ip::tcp::acceptor* acceptor) {
    /* async_accept can throw errors */
    try {
        while (!m_stop_in_operation.load(std::memory_order::acquire)) {
            auto id = m_last_id++;

            auto onSessionDisconnected = [this, id](std::optional<std::exception> ex) {
                on_session_stopped(id, ex);
            };
            auto onData = [this, id](const std::byte* data, size_t size) {
                inform_user_of_data(id, data, size);
            };

            auto sess = std::make_shared<tcp_session>(
                co_await acceptor->async_accept(boost::asio::use_awaitable),
                onData,
                onSessionDisconnected);

            {
                std::unique_lock lock(m_mutex);
                m_sessions.emplace(id, sess);
            }

            if (m_new_session_cb)
                m_new_session_cb(*this, id);

            sess->start();
        }
    } catch (...) { stop_async(); }
}

void tcp_server::listen_on_endpoints() {
    std::lock_guard lock(m_mutex);

    /* exception will be thrown if can't listen to endpoint */
    try {
        for (auto e : m_endpoints) {
            boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip),
                                              static_cast<boost::asio::ip::port_type>(e.port));

            auto acceptor =
                std::make_unique<boost::asio::ip::tcp::acceptor>(m_context->context(), ep);
            boost::asio::co_spawn(
                m_context->context(), listener(acceptor.get()), boost::asio::detached);

            m_acceptors.emplace_back(std::move(acceptor));
        }
    } catch (...) {
        for (auto& acceptor: m_acceptors)
            acceptor->close();
        throw;
    }
}

void tcp_server::on_stop() {
    if (m_on_stopped_listening_cb)
        m_on_stopped_listening_cb(*this);

    m_is_running.store(false, std::memory_order::release);
    m_is_running.notify_all();
}


void tcp_server::inform_user_of_data(session_id_t id, const std::byte* data, size_t size) {
    if (m_on_data_read_user_cb)
        m_on_data_read_user_cb(*this, id, data, size);
}

void tcp_server::on_session_stopped(session_id_t id, std::optional<std::exception> ex) {
    if (m_on_disconnect_user_cb)
        m_on_disconnect_user_cb(*this, id, ex);

    std::unique_lock lock(m_mutex);
    m_sessions.erase(id);
}
}
