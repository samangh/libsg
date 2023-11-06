#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <stddef.h>
#include <string>
#include <vector>

#include "sg/buffer.h"
#include "sg/export/sg_common.h"
#include "sg/pimpl.h"

namespace sg {

class SG_COMMON_EXPORT tcp_listener {
    class impl;

    sg::pimpl<impl> pimpl;

  public:
    typedef size_t client_id;
    typedef size_t write_req_id;
    typedef sg::unique_c_buffer<uint8_t> buffer;

    typedef std::function<void(tcp_listener *, client_id, const std::string &msg)> on_error_cb_t;
    typedef std::function<void(tcp_listener *, client_id)> on_client_connected_cb_t;
    typedef std::function<void(tcp_listener *, client_id)> on_client_disconnected_cb_t;
    typedef std::function<void(tcp_listener *)> on_start_cb_t;
    typedef std::function<void(tcp_listener *)> on_stop_cb_t;
    typedef std::function<void(tcp_listener *, client_id, size_t length)> on_data_available_cb_t;

    /* Consructs a TCP listener, callbacks can be nullptr */
    tcp_listener();
    ~tcp_listener();

    void start(const int port,
               on_error_cb_t on_error_cb,
               on_client_connected_cb_t on_client_connected_cb,
               on_client_disconnected_cb_t on_client_disconnected_cb,
               on_start_cb_t on_start,
               on_stop_cb_t on_stop,
               on_data_available_cb_t on_data_available_cb);
    void stop();
    bool is_running() const;
    size_t number_of_clients() const;

    void write(client_id, sg::shared_opaque_buffer<uint8_t>);

    /* Returns data read from a client */
    std::vector<buffer> get_buffers(client_id);

    /* Returns data read from all clients */
    std::map<client_id, std::vector<buffer>> get_buffers();

    /* Combines all data read from a client as a vector */
    std::vector<uint8_t> get_buffers_as_vector(client_id);

    /* Combines all data read from all client as a set of vector */
    std::map<client_id, std::vector<uint8_t>> get_buffers_as_vector();

    std::vector<uint8_t> buffers_to_vector(std::vector<buffer>);
};

} // namespace sg
