#pragma once

#include <shared_mutex>

namespace udaq::common {

/**
 * @brief Provides thread-safe methods for indicating progress
 */
class ProgressIndicator {
public:
    struct Progress {
        /* current status message */
        std::string message;
        /* current progress */
        float progress;
        /* max value that @ref progress can take */
        float max_val=1;
    };

    /**
     * @brief current progress
     * @return currrent progress
     */
    Progress progress() const;
protected:
    void set_progress(float progress, const std::string& msg);
    void set_percentage(float);
    void set_max_val(float);
    void set_message(const std::string&);
    void reset();
private:
    Progress m_progress;
    mutable std::shared_mutex m_progress_mutex;
};

}
