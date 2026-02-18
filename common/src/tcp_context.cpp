#include "sg/tcp_context.h"

#include "sg/debug.h"

namespace sg::net {

tcp_context::tcp_context(Private, size_t noWorkers, stopped_cb_t onStoppedCallBack) {
    /* setup worker*/
    m_on_stopped_call_back = onStoppedCallBack;

    auto workerTask = std::bind(&tcp_context::on_worker_tick, this, std::placeholders::_1);
    auto stoppedTask = std::bind(&tcp_context::on_worker_stop, this, std::placeholders::_1);

    for (size_t i = 0; i < noWorkers; ++i) {
        auto w = std::make_unique<notifiable_background_worker>(std::chrono::seconds(1), workerTask,
                                                                nullptr, stoppedTask);
        m_workers.emplace_back(std::move(w));
    }
}
tcp_context::~tcp_context() {
    stop_async();
    wait_for_stop();

    future_get_once();
}

std::shared_ptr<tcp_context> tcp_context::create(size_t noWorkers, stopped_cb_t onStoppedCallBack) {
    return std::make_shared<tcp_context>(Private(), noWorkers, onStoppedCallBack);
}

void tcp_context::run(bool enableGuard) {
    if (is_running())
        SG_THROW(std::runtime_error, "this tcp_context is already running");

    if (enableGuard)
        m_guard = std::make_unique<
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(m_io_context_ptr));

    // We set the connection count before, because if the io_context has nothing to do the on_stop
    // handlers may get called before we have started all of tem.
    m_running_worker_threads_count.store(m_workers.size());

    for (auto& worker : m_workers)
        try {
            if (!worker->is_running())
                worker->start();
        } catch (...) {
            --m_running_worker_threads_count;
            throw;
        }
}

boost::asio::io_context& tcp_context::context() { return m_io_context_ptr; }

const boost::asio::io_context& tcp_context::context() const {
    return m_io_context_ptr;
}

bool tcp_context::is_running() const {
    for (const auto& worker: m_workers)
        if (worker->is_running())
            return true;
    return false;
}


void tcp_context::stop_async() {
    if (m_guard)
        reset_guard();

    if (!m_io_context_ptr.stopped())
        m_io_context_ptr.stop();

    for (const auto& worker: m_workers)
        if (worker->is_running())
            worker->request_stop();
}

void tcp_context::wait_for_stop() {
    for (const auto& worker: m_workers)
        worker->wait_for_stop();
}
void tcp_context::reset_guard() {
    if (!is_running())
        return;

    if (!m_guard)
        SG_THROW(std::runtime_error, "this tcp_context has no guard");

    // call guard (not smart_ptr!) reset
    m_guard->reset();
}
void tcp_context::future_get_once() noexcept(false) {
    std::vector<std::string> errors;
    for (const auto& worker: m_workers)
        try {
            worker->future_get_once();
        } catch (const std::exception& e) {
            errors.emplace_back(e.what());
        } catch (...) {
            errors.emplace_back("Unknow error");
        }

    if (!errors.empty())
        throw std::runtime_error(fmt::format("{}", fmt::join(errors, "\n")));
}

void tcp_context::on_worker_tick(notifiable_background_worker* worker) {
    m_io_context_ptr.run();
    worker->request_stop();
}

void tcp_context::on_worker_stop(notifiable_background_worker*) {
    if (--m_running_worker_threads_count == 0)
        if (m_on_stopped_call_back)
            m_on_stopped_call_back(*this);
}
} // namespace sg::net
