#include "udaq/common/progress_indicator.h"

namespace udaq::common
{

ProgressIndicator::Progress ProgressIndicator::progress() const
{
    std::shared_lock lock(m_progress_mutex);
    return m_progress;
}

void ProgressIndicator::set_progress(float progress, const std::string &msg)
{
    std::unique_lock lock(m_progress_mutex);
    m_progress.progress = progress;
    m_progress.message = msg;
}

void ProgressIndicator::set_percentage(float progress)
{
    std::unique_lock lock(m_progress_mutex);
    m_progress.progress = progress;
}

void ProgressIndicator::set_max_val(float max_val)
{
    std::unique_lock lock(m_progress_mutex);
    m_progress.max_val = max_val;
}

void ProgressIndicator::set_message(const std::string &msg)
{
    std::unique_lock lock(m_progress_mutex);
    m_progress.message = msg;
}

void ProgressIndicator::reset()
{
    set_progress(0, "");
}

}
