#include "sg/tcp_context.h"

namespace sg::net {

tcp_context::tcp_context(Private) {
    /* setup worker*/
    auto workerTask = std::bind(&tcp_context::on_worker_tick, this, std::placeholders::_1);
    m_worker = std::make_unique<notifiable_background_worker>(
        std::chrono::seconds(1), workerTask, nullptr, nullptr);
}

std::shared_ptr<tcp_context> tcp_context::create() {
    return std::make_shared<tcp_context>(Private());
}

void tcp_context::run() {
    if (!m_worker->is_running())
        m_worker->start();
}

boost::asio::io_context& tcp_context::context() { return m_io_context_ptr; }

const boost::asio::io_context& tcp_context::context() const {
    return m_io_context_ptr;
}

bool tcp_context::is_running() const { return (m_worker && m_worker->is_running()); }

void tcp_context::on_worker_tick(notifiable_background_worker* worker) {
    m_io_context_ptr.run();
    worker->request_stop();
}
}
