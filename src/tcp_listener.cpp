#include "sg/tcp_listener.h"

#include <uv.h>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <vector>
#include <thread>
#include <memory>

namespace sg {

class SG_COMMON_EXPORT tcp_listener::impl {
public:
    impl(tcp_listener* parent_listener,
         on_error_cb_t on_error_cb,
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
    size_t number_of_clients() const;
    ~impl();
private:
    struct buff {
       std::unique_ptr<uint8_t> data;
       size_t length;
    };
    struct client_data {
        client_data(client_id _id) :
            id_ptr(std::make_unique<client_id>(_id)){}

        std::unique_ptr<client_id> id_ptr;
        std::vector<buff> data;
    };

    static void on_read(uv_stream_t* tcp, ssize_t nread, const uv_buf_t* buf);
    static void on_new_connection(uv_stream_t *stream, int status);
    static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf);
    static void on_client_disconnected(uv_handle_t* handle);
    void on_error(const std::string &message);

    uv_loop_t m_loop;
    std::thread m_thread;

    mutable std::shared_mutex m_mutex;          /* Mutex for getting data*/

    tcp_listener* m_parent_listener;

    on_error_cb_t m_on_error_cb; /* Called in case of errors after connect(...) */
    on_client_connected_cb_t m_on_client_connected_cb;
    on_client_disconnected_cb_t m_on_client_disconnected_cb;
    on_start_cb_t m_on_start_cb;
    on_stop_cb_t m_on_stop_cb;
    on_data_available_cb_t m_on_data_available;

    uint m_client_counter;

    std::map<client_id, client_data> m_clients;

    std::unique_ptr<uv_tcp_t> m_sock; /* Socket used for connection */
    std::unique_ptr<uv_connect_t> m_conn; /* UV connection object */
    std::unique_ptr<uv_async_t> m_async; /* For stopping the loop */
};

/* Call back for freeing handles after they are closed */
void free_handle_on_close(uv_handle_t *handle) {
    if (handle != nullptr)
        delete handle;
}

void close_handle(uv_handle_s *handle, void *) { uv_close(handle, nullptr); }

tcp_listener::impl::impl(tcp_listener* parent_listener, on_error_cb_t on_error_cb, on_client_connected_cb_t on_client_connected_cb,
                           on_client_disconnected_cb_t on_client_disconnected_cb, on_start_cb_t on_start,
                           on_stop_cb_t on_stop, on_data_available_cb_t on_data_available_cb)
    : m_parent_listener(parent_listener),
      m_on_error_cb(on_error_cb),
      m_on_client_connected_cb(on_client_connected_cb),
      m_on_client_disconnected_cb(on_client_disconnected_cb),
      m_on_start_cb(on_start),
      m_on_stop_cb(on_stop),
      m_on_data_available(on_data_available_cb),
      m_client_counter(0) {}

void tcp_listener::impl::start(const int port) {
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
            m_on_start_cb(m_parent_listener);
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
            m_on_stop_cb(m_parent_listener);
    });
}

void tcp_listener::impl::stop() {
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

bool tcp_listener::impl::is_running() const { return m_thread.joinable() && (uv_loop_alive(&m_loop) != 0); }

size_t tcp_listener::impl::number_of_clients() const {
    std::shared_lock lock(m_mutex);
    return m_clients.size();
}

tcp_listener::impl::~impl() {
    if (is_running())
        stop();
}

std::vector<uint8_t> tcp_listener::impl::get_buffer(client_id id) {
    std::vector<buff> buffers;

    /* swap buffers, do this to minimise locking time*/
    {
        std::lock_guard lock(m_mutex);
        m_clients.at(id).data.swap(buffers);
    }

    std::vector<uint8_t> result;
    for (const auto &buf : buffers)
        result.insert(result.end(), buf.data.get(), buf.data.get() + buf.length);

    return result;
}

std::map<tcp_listener::client_id, std::vector<uint8_t>> tcp_listener::impl::get_buffers() {
    std::map<tcp_listener::client_id, std::vector<buff>> buffer_map;

    /* swap buffers, do this to minimise locking time*/
    {
        std::lock_guard lock(m_mutex);
        for (auto& [id, val]: m_clients)
            if (!val.data.empty())
            {
                std::vector<buff> _client_buffers;
                val.data.swap(_client_buffers);
                buffer_map.emplace(id, std::move(_client_buffers));
            }
    }

    std::map<tcp_listener::client_id, std::vector<uint8_t>> result;
    for (auto& [id, buffers]: buffer_map)
        {
            std::vector<uint8_t> merged_buffer;
            for (const auto &buf : buffers)
                merged_buffer.insert(merged_buffer.end(), buf.data.get(), buf.data.get() + buf.length);
            result.emplace(id, std::move(merged_buffer));
        }

    return result;
}

void tcp_listener::impl::on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    auto a = (impl *)client->loop->data;
    auto id = *((client_id*)client->data);

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
        a->m_clients.at(id).data.emplace_back(std::move(b));
    }

    a->m_on_data_available(a->m_parent_listener, id, nread);
}

void tcp_listener::impl::on_new_connection(uv_stream_t *server, int status) {
    auto a = (impl *)(server->loop->data);

    if (status < 0) {
        a->on_error(uv_strerror((int)status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(&(a->m_loop), client);

    {
        std::lock_guard lock(a->m_mutex);
        a->m_client_counter++;

        auto _d = client_data(a->m_client_counter);
        client->data=_d.id_ptr.get();
        a->m_clients.emplace(a->m_client_counter, std::move(_d));
    }

    if (a->m_on_client_connected_cb != nullptr)
        a->m_on_client_connected_cb(a->m_parent_listener, *((client_id*)client->data));

    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_cb, on_read);
    } else {
        /* for some reason we could not accept a connection */
        a->on_error(uv_strerror(status));
        uv_close((uv_handle_t *)client, on_client_disconnected);
    }
}

void tcp_listener::impl:: alloc_cb(uv_handle_t *, size_t size, uv_buf_t *buf) {
    /* Generate buffer for the handle */
    *buf = uv_buf_init((char *)malloc(size), (unsigned int)size);
}

void tcp_listener::impl::on_client_disconnected(uv_handle_t *handle) {
    auto a = (impl *)(handle->loop->data);
    auto id = *((client_id*)handle->data);

    free_handle_on_close(handle);

    if (a->m_on_client_disconnected_cb != nullptr)
        a->m_on_client_disconnected_cb(a->m_parent_listener, id);

    /* this releases all memory associated with the client*/
    {
        std::lock_guard lock(a->m_mutex);
        a->m_clients.erase(id);
    }
}

/************************************************************
 *  PUBLIC INTERFACE
 ***********************************************************/

void tcp_listener::impl::on_error(const std::string &message) {
    if (m_on_error_cb != nullptr)
        m_on_error_cb(m_parent_listener, message);
}

tcp_listener::tcp_listener(on_error_cb_t on_error_cb,
                           on_client_connected_cb_t on_client_connected_cb,
                           on_client_disconnected_cb_t on_client_disconnected_cb,
                           on_start_cb_t on_start,
                           on_stop_cb_t on_stop,
                           on_data_available_cb_t on_data_available_cb):
    pimpl(sg::pimpl<impl>(this,
                          on_error_cb,
                          on_client_connected_cb,
                          on_client_disconnected_cb,
                          on_start,
                          on_stop,
                          on_data_available_cb))
{}

tcp_listener::~tcp_listener() = default;

void tcp_listener::start(const int port)
{
    pimpl->start(port);
}

void tcp_listener::stop()
{
    pimpl->stop();
}

bool tcp_listener::is_running() const
{
    return pimpl->is_running();
}

size_t tcp_listener::number_of_clients() const
{
    return pimpl->number_of_clients();
}

std::vector<uint8_t> tcp_listener::get_buffer(client_id id)
{
    return pimpl->get_buffer(id);
}

std::map<tcp_listener::client_id, std::vector<uint8_t> > tcp_listener::get_buffers()
{
    return pimpl->get_buffers();
}




} // namespace sg
