#include "sg/tcp_listener.h"
#include "sg/buffer.h"
#include "sg/libuv_wrapper.h"

#include <uv.h>

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT tcp_listener::impl : public sg::libuv_wrapper {
  public:
    impl(tcp_listener *parent_listener);

    void start(const int port,
               on_error_cb_t on_error_cb,
               on_client_connected_cb_t on_client_connected_cb,
               on_client_disconnected_cb_t on_client_disconnected_cb,
               libuv_on_start_cb_t on_start,
               libuv_on_stop_cb_t on_stop,
               on_data_available_cb_t on_data_available_cb);
    size_t number_of_clients() const;

    void write(client_id, sg::shared_opaque_buffer<uint8_t> buffer);

    std::vector<sg::tcp_listener::buffer> get_buffer(client_id);
    std::map<client_id, std::vector<tcp_listener::buffer>> get_buffers();

  private:
    struct client_data {
        client_data(impl *_listener, client_id _id)
            : listener(_listener),
              id(_id),                                    //
              uv_tcp_handle(std::make_unique<uv_tcp_t>()) //
        {
            uv_tcp_handle->data = this;
        }

        impl *listener;

        client_id id;
        std::unique_ptr<uv_tcp_t> uv_tcp_handle;
        std::vector<sg::tcp_listener::buffer> data;
    };

    struct write_request {
        write_request(std::shared_ptr<client_data> _client,
                      write_req_id _write_id,
                      sg::shared_opaque_buffer<uint8_t> buffer)
            : write_id(_write_id),
              buffer(buffer),
              client_data_ptr(_client),
              uv_write_req_handle(std::make_unique<uv_write_t>()) {
            uv_write_req_handle->data = this;
        }
        write_req_id write_id;
        sg::shared_opaque_buffer<uint8_t> buffer;
        std::shared_ptr<client_data> client_data_ptr;
        std::unique_ptr<uv_write_t> uv_write_req_handle;
    };

    void remove_write_request(write_req_id);
    write_request *add_write_request(client_id id, sg::shared_opaque_buffer<uint8_t>);

    tcp_listener *m_parent_listener;

    int port;
    void setup_libuv_operations() override;
    void stop_libuv_operations() override {}

    /* libuv callbacks */
    static void on_read(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf);
    static void on_new_connection(uv_stream_t *stream, int status);
    static void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);
    static void on_client_disconnected(uv_handle_t *handle);
    static void on_write(uv_write_s *req, int status);
    void on_error(client_id, const std::string &message);

    /* call backs to user */
    on_error_cb_t m_on_error_cb; /* Called in case of errors after connect(...) */
    on_client_connected_cb_t m_on_client_connected_cb;
    on_client_disconnected_cb_t m_on_client_disconnected_cb;
    on_data_available_cb_t m_on_data_available;

    /* clients*/
    mutable std::shared_mutex m_mutex; /* Mutex for getting modifying m_clients*/
    size_t m_client_counter;
    std::map<client_id, std::shared_ptr<client_data>> m_clients;

    /* write requets*/
    mutable std::shared_mutex m_write_req_mutex; /* Mutex for getting data*/
    write_req_id m_write_request_counter;
    std::map<write_req_id, std::unique_ptr<write_request>> m_write_requests;

    /* libuv stuff*/
    std::unique_ptr<uv_tcp_t> m_sock;     /* Socket used for connection */
    std::unique_ptr<uv_connect_t> m_conn; /* UV connection object */
};

void close_handle(uv_handle_s *handle, void *) { uv_close(handle, nullptr); }

tcp_listener::impl::impl(tcp_listener *parent_listener) : m_parent_listener(parent_listener) {}

void tcp_listener::impl::start(const int port,
                               on_error_cb_t on_error_cb,
                               on_client_connected_cb_t on_client_connected_cb,
                               on_client_disconnected_cb_t on_client_disconnected_cb,
                               libuv_on_start_cb_t on_start,
                               libuv_on_stop_cb_t on_stop,
                               on_data_available_cb_t on_data_available_cb) {

    if (is_running())
        throw std::logic_error("this tcplistener is currently running");

    this->port = port;
    m_on_error_cb = on_error_cb;
    m_on_client_connected_cb = on_client_connected_cb;
    m_on_client_disconnected_cb = on_client_disconnected_cb;
    m_on_data_available = on_data_available_cb;

    start_libuv(on_start, on_stop);
}

size_t tcp_listener::impl::number_of_clients() const {
    std::shared_lock lock(m_mutex);
    return m_clients.size();
}

void tcp_listener::impl::write(client_id id, sg::shared_opaque_buffer<uint8_t> bytes) {
    /* we must move or copy the bytes, as the buffer must be kept until the callback is called */
    auto write_req = add_write_request(id, std::move(bytes));

    uv_buf_t wrbuf = uv_buf_init((char *)write_req->buffer.get(), write_req->buffer.size());

    auto req = write_req->uv_write_req_handle.get();
    auto stream = (uv_stream_t *)write_req->client_data_ptr->uv_tcp_handle.get();
    uv_write(req, stream, &wrbuf, 1, &on_write);
}

std::vector<sg::tcp_listener::buffer> tcp_listener::impl::get_buffer(client_id id) {
    std::vector<sg::tcp_listener::buffer> buffers;

    /* swap buffers, do this to minimise locking time*/
    {
        std::lock_guard lock(m_mutex);
        m_clients.at(id)->data.swap(buffers);
    }

    return buffers;
}

std::map<tcp_listener::client_id, std::vector<tcp_listener::buffer>>
tcp_listener::impl::get_buffers() {
    std::map<tcp_listener::client_id, std::vector<sg::tcp_listener::buffer>> buffer_map;

    /* swap buffers, do this to minimise locking time*/
    std::lock_guard lock(m_mutex);
    for (auto &[id, val] : m_clients)
        if (!val->data.empty()) {
            std::vector<sg::tcp_listener::buffer> _client_buffers;
            val->data.swap(_client_buffers);
            buffer_map.emplace(id, std::move(_client_buffers));
        }

    return buffer_map;
}

void tcp_listener::impl::on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    auto a = ((client_data *)client->data)->listener;
    auto id = ((client_data *)client->data)->id;

    /* Check for disconnection*/
    if (nread < 0) {
        free(buf->base);
        uv_close((uv_handle_t *)client, on_client_disconnected);
        if (nread != UV_EOF)
            a->on_error(id, uv_strerror((int)nread));
        return;
    }

    /* Take ownership of the buffer, as it is now ours. */
    {
        std::lock_guard lock(a->m_mutex);
        auto b = sg::tcp_listener::buffer((uint8_t *)buf->base, nread);
        a->m_clients.at(id)->data.emplace_back(std::move(b));
    }

    if (a->m_on_data_available != nullptr)
        a->m_on_data_available(a->m_parent_listener, id, nread);
}

void tcp_listener::impl::on_new_connection(uv_stream_t *server, int status) {
    auto a = (impl *)(server->loop->data);

    if (status < 0) {
        a->on_error(0, uv_strerror((int)status));
        return;
    }

    client_id id;
    {
        std::lock_guard lock(a->m_mutex);
        id = a->m_client_counter++;
        a->m_clients.emplace(id, std::make_unique<client_data>(a, id));
    }

    uv_tcp_t *client = a->m_clients.at(id)->uv_tcp_handle.get();
    uv_tcp_init(&(a->m_loop), client);

    if (a->m_on_client_connected_cb != nullptr)
        a->m_on_client_connected_cb(a->m_parent_listener, id);

    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        uv_read_start((uv_stream_t *)client, alloc_cb, on_read);
    } else {
        /* for some reason we could not accept a connection */
        a->on_error(id, uv_strerror(status));
        uv_close((uv_handle_t *)client, on_client_disconnected);
    }
}

void tcp_listener::impl::alloc_cb(uv_handle_t *, size_t size, uv_buf_t *buf) {
    /* Generate buffer for the handle */
    *buf = uv_buf_init((char *)malloc(size), (unsigned int)size);
}

void tcp_listener::impl::on_client_disconnected(uv_handle_t *handle) {
    impl *a = ((client_data *)handle->data)->listener;
    auto id = ((client_data *)handle->data)->id;

    if (a->m_on_client_disconnected_cb != nullptr)
        a->m_on_client_disconnected_cb(a->m_parent_listener, id);

    /* this releases all memory associated with the client*/
    {
        std::lock_guard lock(a->m_mutex);
        a->m_clients.erase(id);
    }
}

void tcp_listener::impl::on_write(uv_write_s *req, int status) {
    impl *a = ((write_request *)req->data)->client_data_ptr->listener;
    auto _client_id = ((write_request *)req->data)->client_data_ptr->id;
    auto _write_id = ((write_request *)req->data)->write_id;

    if (status < 0)
        a->on_error(_client_id, uv_strerror((int)status));

    a->remove_write_request(_write_id);
}

void tcp_listener::impl::remove_write_request(write_req_id id) {
    std::lock_guard lock(m_mutex);
    m_write_requests.erase(id);
}

tcp_listener::impl::write_request *
tcp_listener::impl::add_write_request(client_id id, sg::shared_opaque_buffer<uint8_t> buffer) {
    std::lock_guard lock(m_mutex);

    auto req_number = m_write_request_counter++;
    auto req = std::make_unique<write_request>(m_clients.at(id), req_number, buffer);
    m_write_requests.emplace(req_number, std::move(req));

    return m_write_requests.at(req_number).get();
}

void tcp_listener::impl::setup_libuv_operations() {
    // Clean previous use data
    {
        std::lock_guard lock(m_mutex);
        m_write_request_counter = 0;
        m_client_counter = 0;
        m_clients.clear();
        m_write_requests.clear();
    }

    /* Create address */
    struct sockaddr_in dest;
    THROW_ON_LIBUV_ERROR(uv_ip4_addr("0.0.0.0", port, &dest));

    /* Create socket */
    m_sock = std::make_unique<uv_tcp_t>();
    THROW_ON_LIBUV_ERROR(uv_tcp_init(&m_loop, m_sock.get()));

    /* Bind socket and address */
    THROW_ON_LIBUV_ERROR(uv_tcp_bind(m_sock.get(), (const struct sockaddr *)&dest, 0));

    /* Start listening */
    THROW_ON_LIBUV_ERROR(uv_listen((uv_stream_t *)m_sock.get(), 20, on_new_connection));
}

/************************************************************
 *  PUBLIC INTERFACE
 ***********************************************************/

void tcp_listener::impl::on_error(client_id id, const std::string &message) {
    if (m_on_error_cb != nullptr)
        m_on_error_cb(m_parent_listener, id, message);
}

tcp_listener::tcp_listener() : pimpl(sg::pimpl<impl>(this)) {}

tcp_listener::~tcp_listener() {
    pimpl->stop();
}


void tcp_listener::start(const int port,
                         on_error_cb_t on_error_cb,
                         on_client_connected_cb_t on_client_connected_cb,
                         on_client_disconnected_cb_t on_client_disconnected_cb,
                         on_start_cb_t on_start,
                         on_stop_cb_t on_stop,
                         on_data_available_cb_t on_data_available_cb) {

    auto started_cb = [&, on_start](libuv_wrapper *) {
        if (on_start)
            on_start(this);
    };

    auto stopped_cb = [&, on_stop](libuv_wrapper *) {
        if (on_stop)
            on_stop(this);
    };

    pimpl->start(port,
                 on_error_cb,
                 on_client_connected_cb,
                 on_client_disconnected_cb,
                 started_cb,
                 stopped_cb,
                 on_data_available_cb);
}

void tcp_listener::stop() { pimpl->stop(); }

bool tcp_listener::is_running() const { return pimpl->is_running(); }

size_t tcp_listener::number_of_clients() const { return pimpl->number_of_clients(); }

void tcp_listener::write(client_id id, sg::shared_opaque_buffer<uint8_t> bytes) {
    pimpl->write(id, std::move(bytes));
}

std::vector<tcp_listener::buffer> tcp_listener::get_buffers(client_id id) {
    return pimpl->get_buffer(id);
}

std::vector<uint8_t> tcp_listener::get_buffers_as_vector(client_id id) {
    auto client_buffers = this->get_buffers(id);
    return tcp_listener::buffers_to_vector(std::move(client_buffers));
}

std::map<tcp_listener::client_id, std::vector<uint8_t>> tcp_listener::get_buffers_as_vector() {
    auto buffer_map = this->get_buffers();
    std::map<tcp_listener::client_id, std::vector<uint8_t>> result;

    for (auto &[id, buffers] : buffer_map) {
        auto vec = tcp_listener::buffers_to_vector(std::move(buffers));
        result.emplace(id, std::move(vec));
    }

    return result;
}

std::vector<uint8_t> tcp_listener::buffers_to_vector(std::vector<buffer> buffers) {
    size_t total_size = 0;
    for (const auto &buf : buffers)
        total_size += buf.size();

    std::vector<uint8_t> result;
    result.reserve(total_size);

    for (auto &buf : buffers)
        result.insert(result.end(), buf.begin(), buf.end());

    return result;
}

std::map<tcp_listener::client_id, std::vector<tcp_listener::buffer>> tcp_listener::get_buffers() {
    return pimpl->get_buffers();
}

} // namespace sg
