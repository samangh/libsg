#pragma once

#include "pimpl.h"

#include <filesystem>
#include <functional>
#include <string>

namespace sg {

class file_writer {
    class impl;
    sg::pimpl<impl> pimpl;

  public:
    typedef std::function<void(const std::string &msg)> error_cb_t;
    typedef std::function<void(void)> started_cb_t;
    typedef std::function<void(void)> stopped_cb_t;

    file_writer();
    ~file_writer();
    void start(std::filesystem::path _path,
               error_cb_t on_error_cb,
               started_cb_t on_client_connected_cb,
               stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);
    void stop();
    bool is_running() const;

    void write(const char *data, size_t length);
    inline void write(const std::string &msg) {
        /* having this in header means that we don't have to
         * worry about passing std::string across library
         * boundaries. */
        write(msg.c_str(), msg.size());
    }
    inline void write_line(const std::string &msg) {
        /* having this in header means that we don't have to
         * worry about passing std::string across library
         * boundaries. */
        write(msg + "\n");
    }

    std::filesystem::path path() const;
};

} // namespace sg
