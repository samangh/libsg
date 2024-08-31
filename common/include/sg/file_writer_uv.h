#pragma once

#include "pimpl.h"
#include "sg/buffer.h"
#include "sg/memory.h"
#include "sg/export/common.h"

#include <filesystem>
#include <functional>
#include <string>

namespace sg {

class SG_COMMON_EXPORT file_writer_uv: public sg::enable_lifetime_indicator {
    class impl;
    sg::pimpl<impl> pimpl;

  public:
    typedef std::function<void(const std::string &msg)> error_cb_t;
    typedef std::function<void(file_writer_uv *)> stopped_cb_t;

    file_writer_uv();
    ~file_writer_uv();
    void start(const std::filesystem::path &_path,
               error_cb_t on_error_cb,
               stopped_cb_t on_client_disconnected_cb,
               unsigned int write_interval = 1000);
    void stop();
    bool is_running() const;

    void write_async(sg::shared_c_buffer<std::byte>);
    void write(sg::shared_c_buffer<std::byte>);

    /* note: the data is copied internally, the data can be freed after
     * calling this function */
    void write_async(const char *data, size_t length);

    /* note: the data is copied internally */
    void write_async(const std::string_view &);

    /* note: the data is copied internally */
    void write_line_async(const std::string_view &);

    std::filesystem::path path() const;
};

} // namespace sg
