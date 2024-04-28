#pragma once

#include "sg/buffer.h"
#include "sg/export/sg_common.h"
#include "sg/libuv_wrapper.h"
#include "sg/pimpl.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT tcp_client {
    class impl;
    sg::pimpl<impl> pimpl;
  public:
    typedef sg::shared_c_buffer<uint8_t> buffer_t;

    typedef std::function<void(tcp_client *, const std::string &msg)> on_error_cb_t;
    typedef std::function<void(tcp_client *)> on_connected_cb_t;
    typedef std::function<void(tcp_client *)> on_disconnected_cb_t;
    typedef std::function<void(tcp_client *, size_t length)> on_data_available_cb_t;

    tcp_client();
    tcp_client(std::shared_ptr<sg::libuv_wrapper>);
    ~tcp_client();

    void connect(const int port,
                 const std::string &address);

    void connect_async(const int port,
                       const std::string &address,
                       on_error_cb_t on_error_cb,
                       on_connected_cb_t on_connected_cb,
                       on_disconnected_cb_t on_disconnected_cb,
                       on_data_available_cb_t on_data_available_cb);

    void disconnect();
    void disconnect_async();

    void write(const std::string);
    void write(buffer_t);
    void write(std::vector<buffer_t>);

    void write_async(const std::string);
    void write_async(buffer_t);
    void write_async(std::vector<buffer_t>);

    std::vector<uint8_t> read(size_t count);
    std::vector<uint8_t> read(const std::string& end);
    void start_read_async();
    void stop_read_async();

    std::vector<buffer_t> get_buffers();
    std::vector<uint8_t> buffers_to_vector(std::vector<buffer_t>);
};

class SG_COMMON_EXPORT tcp_client_old {
    class impl_old;
    sg::pimpl<impl_old> pimpl;

  public:
    typedef sg::shared_c_buffer<uint8_t> buffer_t;

    typedef std::function<void(tcp_client_old *, const std::string &msg)> on_error_cb_t;
    typedef std::function<void(tcp_client_old *)> on_connected_cb_t;
    typedef std::function<void(tcp_client_old *)> on_disconnected_cb_t;
    typedef std::function<void(tcp_client_old *, size_t length)> on_data_available_cb_t;

    tcp_client_old();
    ~tcp_client_old();

    void connect(const int port,
               const std::string& address,
               on_error_cb_t on_error_cb,
               on_connected_cb_t on_connected_cb,
               on_disconnected_cb_t on_disconnected_cb,
               on_data_available_cb_t on_data_available_cb);

    void disconnect();
    bool is_connected() const;
    bool is_connected_or_connecting() const;

    void write(buffer_t);
    void write(std::vector<buffer_t>);

    /* Returns data read from a client */
    std::vector<buffer_t> get_buffers();

    std::vector<uint8_t> buffers_to_vector(std::vector<buffer_t>);
};

} // namespace sg
