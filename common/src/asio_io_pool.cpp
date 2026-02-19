#include "sg/asio_io_pool.h"
#include "sg/debug.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace sg::net {

asio_io_pool::asio_io_pool(Private, size_t noWorkers, stopped_cb_t onStoppedCallBack) {
    /* setup worker*/
    m_on_stopped_call_back = onStoppedCallBack;

    auto workerTask = std::bind(&asio_io_pool::on_worker_tick, this, std::placeholders::_1);
    auto stoppedTask = std::bind(&asio_io_pool::on_worker_stop, this, std::placeholders::_1);

    for (size_t i = 0; i < noWorkers; ++i) {
        auto w = std::make_unique<notifiable_background_worker>(std::chrono::seconds(1), workerTask,
                                                                nullptr, stoppedTask);
        m_workers.emplace_back(std::move(w));
    }
}
asio_io_pool::~asio_io_pool() {
    stop_async();
    wait_for_stop();

    future_get_once();
}

std::shared_ptr<asio_io_pool> asio_io_pool::create(size_t noWorkers, stopped_cb_t onStoppedCallBack) {
    return std::make_shared<asio_io_pool>(Private(), noWorkers, onStoppedCallBack);
}

void asio_io_pool::run(bool enableGuard) {
    if (is_running())
        SG_THROW(std::runtime_error, "this asio_io_pool is already running");

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

boost::asio::io_context& asio_io_pool::context() { return m_io_context_ptr; }

const boost::asio::io_context& asio_io_pool::context() const {
    return m_io_context_ptr;
}

bool asio_io_pool::is_running() const {
    for (const auto& worker: m_workers)
        if (worker->is_running())
            return true;
    return false;
}


void asio_io_pool::stop_async() {
    if (m_guard)
        reset_guard();

    if (!m_io_context_ptr.stopped())
        m_io_context_ptr.stop();

    for (const auto& worker: m_workers)
        if (worker->is_running())
            worker->request_stop();
}

void asio_io_pool::wait_for_stop() {
    for (const auto& worker: m_workers)
        worker->wait_for_stop();
}
void asio_io_pool::reset_guard() {
    if (!is_running())
        return;

    if (!m_guard)
        SG_THROW(std::runtime_error, "this asio_io_pool has no guard");

    // call guard (not smart_ptr!) reset
    m_guard->reset();
}
void asio_io_pool::future_get_once() noexcept(false) {
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

void asio_io_pool::on_worker_tick(notifiable_background_worker* worker) {
    m_io_context_ptr.run();
    worker->request_stop();
}

void asio_io_pool::on_worker_stop(notifiable_background_worker*) {
    if (--m_running_worker_threads_count == 0)
        if (m_on_stopped_call_back)
            m_on_stopped_call_back(*this);
}
} // namespace sg::net
