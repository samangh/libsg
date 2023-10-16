#pragma once

#include <uv.h>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <functional>
#include <memory>
#include <cstdint>
#include <map>

namespace sg {

class tcp_listener {
public:
    typedef uint client_id;
    typedef std::function<void(tcp_listener*, const std::string &msg)> on_error_cb_t;
    typedef std::function<void(tcp_listener*, client_id)> on_client_connected_cb_t;
    typedef std::function<void(tcp_listener*, client_id)> on_client_disconnected_cb_t;
    typedef std::function<void(tcp_listener*)> on_start_cb_t;
    typedef std::function<void(tcp_listener*)> on_stop_cb_t;
    typedef std::function<void(tcp_listener*, client_id, size_t length)> on_data_available_cb_t; /* Callback should NOT free the buffer */

    tcp_listener(on_error_cb_t on_error_cb,
                       on_client_connected_cb_t on_client_connected_cb,
                       on_client_disconnected_cb_t on_client_disconnected_cb,
                       on_start_cb_t on_start,
                       on_stop_cb_t on_stop,
                       on_data_available_cb_t on_data_available_cb);

    std::vector<uint8_t> get_buffer(client_id);
    std::map<client_id, std::vector<uint8_t>> get_buffers();

    void start(const int port);
    void stop();
    bool is_running() const;
    uint  number_of_clients() const;
    ~tcp_listener();

private:
    struct buff {
       std::unique_ptr<uint8_t> data;
       size_t length;
    };

    /* this is store in the data pointer of client uv_tcp_t */
    typedef struct client_data {
        client_id id;
    } client_data;

    static void on_read(uv_stream_t* tcp, ssize_t nread, const uv_buf_t* buf);
    static void on_new_connection(uv_stream_t *stream, int status);
    static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf);
    static void on_client_disconnected(uv_handle_t* handle);
    void on_error(const std::string &message);

    uv_loop_t m_loop;
    std::thread m_thread;

    mutable std::shared_mutex m_mutex;          /* Mutex for getting data*/

    on_error_cb_t m_on_error_cb; /* Called in case of errors after connect(...) */
    on_client_connected_cb_t m_on_client_connected_cb;
    on_client_disconnected_cb_t m_on_client_disconnected_cb;
    on_start_cb_t m_on_start_cb;
    on_stop_cb_t m_on_stop_cb;
    on_data_available_cb_t m_on_data_available;

    uint m_number_of_connected_clients;
    uint m_client_counter;

    std::map<client_id, std::vector<buff>> m_client_buffers;

    std::unique_ptr<uv_tcp_t> m_sock; /* Socket used for connection */
    std::unique_ptr<uv_connect_t> m_conn; /* UV connection object */
    std::unique_ptr<uv_async_t> m_async; /* For stopping the loop */
};


}
