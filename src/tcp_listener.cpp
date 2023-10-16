#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <uv.h>
#include <vector>

#include "sg/bytes.h"
#include "sg/tcp_listener.h"

namespace sg {

/* Call back for freeing handles after they are closed */
void free_handle_on_close(uv_handle_t *handle) {
    if (handle != nullptr)
        delete handle;
}

void close_handle(uv_handle_s *handle, void *) { uv_close(handle, nullptr); }

tcp_listener::tcp_listener(on_error_cb_t on_error_cb, on_client_connected_cb_t on_client_connected_cb,
                           on_client_disconnected_cb_t on_client_disconnected_cb, on_start_cb_t on_start,
                           on_stop_cb_t on_stop, on_data_available_cb_t on_data_available_cb)
    : m_on_error_cb(on_error_cb), m_on_client_connected_cb(on_client_connected_cb),
      m_on_client_disconnected_cb(on_client_disconnected_cb), m_on_start_cb(on_start), m_on_stop_cb(on_stop),
      m_on_data_available(on_data_available_cb), m_number_of_connected_clients(0) {}

void tcp_listener::start(const int port) {
    /* setup UV loop */
    uv_loop_init(&m_loop);
    m_loop.data = this;

    int err = 0;

    /* Setup handle for stopping the loop */
    m_async = std::make_unique<uv_async_t>();
    uv_async_init(&m_loop, m_async.get(), [](uv_async_t *handle) { uv_stop(handle->loop); });

    /* Create address */
    struct sockaddr_in dest;
    err = uv_ip4_addr("0.0.0.0", port, &dest);
    if (err != 0)
        throw std::runtime_error(uv_strerror(err));

    /* Create socket */
    m_sock = std::make_unique<uv_tcp_t>();
    err = uv_tcp_init(&m_loop, m_sock.get());
    if (err != 0)
        throw std::runtime_error(uv_strerror(err));

    /* Bind socket and address */
    err = uv_tcp_bind(m_sock.get(), (const struct sockaddr *)&dest, 0);
    if (err != 0)
        throw std::runtime_error(uv_strerror(err));

    /* Start listening */
    err = uv_listen((uv_stream_t *)m_sock.get(), 20, on_new_connection);
    if (err != 0)
        throw std::runtime_error(uv_strerror(err));

    m_thread = std::thread([&]() {
        if (m_on_start_cb != nullptr)
            m_on_start_cb(this);
        while (true) {
            uv_run(&m_loop, UV_RUN_DEFAULT);

            /* The following loop closing logic is from guidance from
             * https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly
             *
             *  If there are any loops that are not closing:
             *
             *  - Use uv_walk and call uv_close on the handles;
             *  - Run the loop again with uv_run so all close callbacks are
             *    called and you can free the memory in the callbacks */

            /* Close callbacks */
            uv_walk(&m_loop, &close_handle, nullptr);

            /* Check if there are any remaining call backs*/
            if (uv_loop_close(&m_loop) != UV_EBUSY)
                break;
        }
        if (m_on_stop_cb != nullptr)
            m_on_stop_cb(this);
    });
}

void tcp_listener::stop() {
    // Signal the thread that it should stop
    // m_end = true;

    if (uv_loop_alive(&m_loop))
        uv_async_send(m_async.get());

    // join and wait for the thread to end.
    // Note: A thread that has finished executing code, but has not yet been joined
    //        is still considered an active thread of execution and is therefore joinable.
    if (m_thread.joinable())
        m_thread.join();
}

bool tcp_listener::is_running() const { return m_thread.joinable() && (uv_loop_alive(&m_loop) != 0); }

int tcp_listener::number_of_clients() const {
    std::shared_lock lock(m_mutex);
    return m_number_of_connected_clients;
}

tcp_listener::~tcp_listener() {
    if (is_running())
        stop();
}

std::vector<uint8_t> tcp_listener::get_buffer() {
    std::vector<buff> buffers;

    /* swap buffers, do this to minimise locking time*/
    {
        std::lock_guard lock(m_mutex);
        m_buffer.swap(buffers);
    }

    std::vector<uint8_t> result;
    for (const auto &buf : buffers)
        result.insert(result.end(), buf.data.get(), buf.data.get() + buf.length);

    return result;
}

void tcp_listener::on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    auto a = (tcp_listener *)client->loop->data;

    /* Check for disconnection*/
    if (nread < 0) {
        free(buf->base);
        uv_close((uv_handle_t *)client, on_client_disconnected);
        if (nread != UV_EOF)
            a->on_error(uv_strerror((int)nread));
        return;
    }

    /* Take ownership of the buffer, as it is now ours. */
    {
        std::lock_guard lock(a->m_mutex);
        buff b;
        b.data = std::unique_ptr<uint8_t>((uint8_t *)buf->base);
        b.length = nread;
        a->m_buffer.emplace_back(std::move(b));
    }

    a->m_on_data_available(a, nread);
}

void tcp_listener::on_new_connection(uv_stream_t *server, int status) {
    auto a = (tcp_listener *)(server->loop->data);

    if (status < 0) {
        a->on_error(uv_strerror((int)status));
        return;
    }

    {
        std::lock_guard lock(a->m_mutex);
        a->m_number_of_connected_clients++;
    }
    if (a->m_on_client_connected_cb != nullptr)
        a->m_on_client_connected_cb(a);

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(&(a->m_loop), client);

    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_cb, on_read);
    } else {
        /* for some reason we could not accept a connection */
        a->on_error(uv_strerror(status));
        uv_close((uv_handle_t *)client, on_client_disconnected);
    }
}

void tcp_listener:: alloc_cb(uv_handle_t *, size_t size, uv_buf_t *buf) {
    /* Generate buffer for the handle */
    *buf = uv_buf_init((char *)malloc(size), (unsigned int)size);
}

void tcp_listener::on_client_disconnected(uv_handle_t *handle) {
    auto a = (tcp_listener *)(handle->loop->data);
    free_handle_on_close(handle);

    {
        std::lock_guard lock(a->m_mutex);
        a->m_number_of_connected_clients--;
    }
    if (a->m_on_client_disconnected_cb != nullptr)
        a->m_on_client_disconnected_cb(a);
}

void tcp_listener::on_error(const std::string &message) {
    if (m_on_error_cb != nullptr)
        m_on_error_cb(this, message);
}

} // namespace sg
