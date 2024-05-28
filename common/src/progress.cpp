#include "sg/progress.h"

#include <mutex>

namespace sg {

Progress ProgressSource::progress() const {
    std::shared_lock lock(m_progress_mutex);
    return m_progress;
}

void ProgressSource::set_progress(float progress, const std::string &msg) {
    std::unique_lock lock(m_progress_mutex);
    m_progress.progress = progress;
    m_progress.message = msg;
}

void ProgressSource::set_progress(float progress) {
    std::unique_lock lock(m_progress_mutex);
    m_progress.progress = progress;
}

void ProgressSource::set_max_val(float max_val) {
    std::unique_lock lock(m_progress_mutex);
    m_progress.max_val = max_val;
}

void ProgressSource::set_message(const std::string &msg) {
    std::unique_lock lock(m_progress_mutex);
    m_progress.message = msg;
}

void ProgressSource::reset() { set_progress(0, ""); }

ProgressIndicator::ProgressIndicator(const ProgressSource &source) : m_source(source) {}

Progress ProgressIndicator::progress() const { return m_source.progress(); }

} // namespace sg
