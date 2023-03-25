#pragma once

#include <shared_mutex>

namespace sg {

struct Progress {
    /** @brief current status message */
    std::string message;
    /** @brief current progress */
    float progress;
    /** @brief max value that @ref progress can take */
    float max_val=1;
};

/** @brief Source for @ref ProgressIndicator */
class ProgressSource {
public:
    Progress progress() const;

    void set_progress(float progress, const std::string& msg);
    void set_percentage(float);
    void set_max_val(float);
    void set_message(const std::string&);
    void reset();
private:
    Progress m_progress;
    mutable std::shared_mutex m_progress_mutex;
};

/** @brief Returns the progresss of a progress source */
class ProgressIndicator {
public:
    ProgressIndicator (const ProgressSource& source);

    /** @brief current progress */
    Progress progress() const;
private:
    const ProgressSource& m_source;
};



}
