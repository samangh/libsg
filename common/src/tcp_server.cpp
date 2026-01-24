#include "sg/tcp_server.h"
#include "sg/tcp_native.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

namespace sg::net {

tcp_server::~tcp_server() noexcept(false) {
    if (m_worker && m_worker->is_running()) {
        stop_async();
        m_worker->future_get_once();
    }
}

void tcp_server::stop_async() {
    /* if stop thread not started, start */
    if (m_stop_in_operation.exchange(true))
        return;

    m_stopping_thread = std::jthread([this]() {
        disconnect_all();

        /* wait untill all clients disconnected and all callbacks called, etc */
        while (clients_count() != 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (!m_io_context_ptr.stopped())
            m_io_context_ptr.stop();
        m_worker->wait_for_stop();
    });
}

void tcp_server::start(std::vector<end_point> endpoints,
                       started_listening_cb_t onStartListening,
                       stopped_listening_cb_t onStopListeniing,
                       session_created_cb_t onNewSession,
                       session_data_available_cb_t onDataAvailCb,
                       session_disconnected_cb_t onDisconnCb) noexcept(false) {
    if (m_worker && m_worker->is_running())
        throw std::runtime_error("tcp_server is already running");

    m_on_started_listening_cb = onStartListening;
    m_on_stopped_listening_cb = onStopListeniing;
    m_new_session_cb = std::move(onNewSession), m_on_data_read_user_cb = std::move(onDataAvailCb);
    m_on_disconnect_user_cb = std::move(onDisconnCb);

    m_stop_in_operation.store(false);

    m_endpoints = endpoints;
    m_last_id = 0;
    m_acceptors.clear();

    m_promise_started_listening = std::promise<void>();

    auto serverThreadTask = std::bind(&tcp_server::on_worker_tick, this, std::placeholders::_1);
    auto serverSetupTask = std::bind(&tcp_server::on_worker_start, this, std::placeholders::_1);
    auto serverStopTask = std::bind(&tcp_server::on_worker_stop, this, std::placeholders::_1);

    m_worker = std::make_unique<notifiable_background_worker>(
        std::chrono::seconds(1), serverThreadTask, serverSetupTask, serverStopTask);
    m_worker->start();
}

void tcp_server::future_get_once() noexcept(false) { m_worker->future_get_once(); }

bool tcp_server::is_stopped() const {
    if (m_worker)
        return !(m_worker->is_running());

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
    for (auto [_, sess] : m_sessions) sess->stop_async();
}

tcp_server::ptr tcp_server::session(session_id_t id) {
    std::shared_lock lock(m_mutex);
    return m_sessions.at(id);
}
void tcp_server::set_keepalive(bool enableKeepAlive, uint32_t idleSec, uint32_t intervalSec,
                               uint32_t count) {
    for (auto a: m_acceptors) {
        sg::net::native::set_keepalive(a->native_handle(), enableKeepAlive, idleSec, intervalSec, count);
    }

}
void tcp_server::set_timeout(uint32_t timeoutMSec) {
    for (auto a: m_acceptors)
        sg::net::native::set_timeout(a->native_handle(), timeoutMSec);
}

boost::asio::awaitable<void>
tcp_server::listener(std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor) {
    while (true) {
        auto id = m_last_id++;

        auto onSessionDisconnected = [this, id](std::optional<std::exception> ex) {
            on_session_stopped(id, ex);
        };
        auto onData = [this, id](const std::byte* data, size_t size) {
            inform_user_of_data(id, data, size);
        };

        auto sess = std::make_shared<tcp_session>(
            co_await acceptor.get()->async_accept(boost::asio::use_awaitable),
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
}

void tcp_server::on_worker_start(notifiable_background_worker*) {
    for (auto e : m_endpoints) {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::make_address(e.ip), e.port);

        auto a = std::make_shared<boost::asio::ip::tcp::acceptor>(m_io_context_ptr, ep);
        boost::asio::co_spawn(m_io_context_ptr, listener(a), boost::asio::detached);
        m_acceptors.push_back(a);
    }

    if (m_on_started_listening_cb)
        m_on_started_listening_cb(*this);
}

void tcp_server::on_worker_stop(notifiable_background_worker*) {
    if (m_on_stopped_listening_cb)
        m_on_stopped_listening_cb(*this);
}

void tcp_server::on_worker_tick(notifiable_background_worker* worker) {
    m_io_context_ptr.run();
    worker->request_stop();
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
