#pragma once

#include "pimpl.h"
#include "sg/export/sg_common.h"

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT tcp_listener {
    class impl;
    sg::pimpl<impl> pimpl;
  public:
    typedef uint client_id;
    typedef uint write_req_id;

    typedef std::function<void(tcp_listener *, client_id, const std::string &msg)> on_error_cb_t;
    typedef std::function<void(tcp_listener *, client_id)> on_client_connected_cb_t;
    typedef std::function<void(tcp_listener *, client_id)> on_client_disconnected_cb_t;
    typedef std::function<void(tcp_listener *)> on_start_cb_t;
    typedef std::function<void(tcp_listener *)> on_stop_cb_t;
    typedef std::function<void(tcp_listener *, client_id, size_t length)> on_data_available_cb_t;

    tcp_listener(on_error_cb_t on_error_cb, on_client_connected_cb_t on_client_connected_cb,
                 on_client_disconnected_cb_t on_client_disconnected_cb, on_start_cb_t on_start, on_stop_cb_t on_stop,
                 on_data_available_cb_t on_data_available_cb);
    ~tcp_listener();

    void start(const int port);
    void stop();
    bool is_running() const;
    size_t number_of_clients() const;

    void write(client_id, std::vector<uint8_t> bytes);

    std::vector<uint8_t> get_buffer(client_id);
    std::map<client_id, std::vector<uint8_t>> get_buffers();
};

} // namespace sg
