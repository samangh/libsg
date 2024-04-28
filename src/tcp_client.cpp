#include "sg/tcp_client.h"
#include "sg/buffer.h"
#include "sg/libuv_wrapper.h"

#include <future>

namespace sg {

class SG_COMMON_EXPORT sg::tcp_client::impl : public sg::enable_lifetime_indicator {
    tcp_client *m_parent_client;

    std::atomic<bool> m_connected = false;

    /* libuv stuff*/
    std::shared_ptr<sg::libuv_wrapper> m_libuv;
    std::unique_ptr<uv_tcp_t> m_sock;     /* Socket used for connection */
    std::unique_ptr<uv_connect_t> m_conn; /* UV connection object */

    /* call backs to user */
    on_error_cb_t m_on_error_cb; /* Called in case of errors after connect(...) */
    on_connected_cb_t m_on_connected_cb;
    on_disconnected_cb_t m_on_disconnected_cb;
    on_data_available_cb_t m_on_data_available;

    void on_error(const std::string &message) {
        if (m_on_error_cb)
            m_on_error_cb(m_parent_client, message);
    }
    static void on_new_connection(uv_connect_t *connection, int status) {
        auto a = (impl *)(connection->data);

        if (status < 0) {
            a->on_error(uv_strerror((int)status));
            a->disconnect_async();
            return;
        }

        a->m_connected = true;
        if (a->m_on_connected_cb)
            a->m_on_connected_cb(a->m_parent_client);
    }
    static void on_tcp_handle_closed(uv_handle_t *connection) {
        auto a = (impl *)(connection->data);

        a->m_connected=false;
        if (a->m_on_disconnected_cb)
            (a->m_on_disconnected_cb(a->m_parent_client));
     }

    public:
      impl(tcp_client *parent_client)
          : m_parent_client(parent_client),
            m_libuv(sg::get_global_uv_holder())
      {
      }


      void connect_async(const int port,
                         const std::string &address,
                         on_error_cb_t on_error_cb,
                         on_connected_cb_t on_connected_cb,
                         on_disconnected_cb_t on_disconnected_cb) {

          m_on_error_cb = on_error_cb;
          m_on_connected_cb = on_connected_cb;
          m_on_disconnected_cb = on_disconnected_cb;
          auto conn =
              sg::libuv_wrapper::cb_t(get_lifetime_indicator(), [&](sg::libuv_wrapper *wrap) {
                  /* Create address */
                  struct sockaddr_in dest;
                  THROW_ON_LIBUV_ERROR(uv_ip4_addr(address.c_str(), port, &dest));

                  /* Create socket */
                  m_sock = std::make_unique<uv_tcp_t>();
                  THROW_ON_LIBUV_ERROR(uv_tcp_init(wrap->get_uv_loop(), m_sock.get()));

                  /* Enable TCP keep-alive */
                  THROW_ON_LIBUV_ERROR(uv_tcp_keepalive(m_sock.get(), 1, 60));

                  m_conn = std::make_unique<uv_connect_t>();
                  m_conn->data = this;
                  THROW_ON_LIBUV_ERROR(uv_tcp_connect(m_conn.get(),
                                                      m_sock.get(),
                                                      (const struct sockaddr *)&dest,
                                                      on_new_connection));
              });

          m_libuv->start_task(conn, nullptr);
      }

      void connect(const int port, const std::string &address)
      {
          struct result {
              std::string msg;
              bool has_error;
          };

          auto promise = std::promise<result>();
          std::future<result> result_future(promise.get_future());

          auto _disconnected_promise = std::promise<void>();
          std::future<void> disconnected_future(_disconnected_promise.get_future());

          on_error_cb_t _on_err = [&promise](tcp_client*, const std::string& msg){
              result res;
              res.has_error = true;
              res.msg = msg;
              promise.set_value(res);
          };

          on_connected_cb_t _on_conn = [&promise](tcp_client*){
              result res;
              res.has_error = false;
              promise.set_value(res);
          };

          on_disconnected_cb_t _on_disconn = [&_disconnected_promise](tcp_client*){
              _disconnected_promise.set_value();
          };

          connect_async(port, address, _on_err, _on_conn, _on_disconn);

          auto res = result_future.get();

          if (res.has_error)
          {
            disconnected_future.wait();
            throw std::runtime_error(res.msg);
          }

          return;
      }

      void disconnect_async() {
          m_conn->handle->data = this;
          uv_close((uv_handle_t*)m_conn->handle, on_tcp_handle_closed);
      }

};


class SG_COMMON_EXPORT sg::tcp_client_old::impl_old : public sg::libuv_wrapper {
    tcp_client_old *m_parent_client;

    int port;
    std::string m_address;

    /* call backs to user */
    on_error_cb_t m_on_error_cb; /* Called in case of errors after connect(...) */
    on_connected_cb_t m_on_connected_cb;
    on_disconnected_cb_t m_on_disconnected_cb;
    on_data_available_cb_t m_on_data_available;

    /* libuv stuff*/
    std::unique_ptr<uv_tcp_t> m_sock;     /* Socket used for connection */
    std::unique_ptr<uv_connect_t> m_conn; /* UV connection object */

    /* read data */
    std::vector<sg::tcp_client_old::buffer_t> data;
    mutable std::mutex m_read_data_mutex;

    /* write data */
    std::vector<std::vector<buffer_t>> m_write_data;
    std::vector<std::unique_ptr<uv_write_t>> m_write_requests;
    mutable std::mutex m_write_data_mutex;

    std::atomic<bool> m_connected = false;

    /* libuv callbacks */
    void on_error(const std::string &message) {
        if (m_on_error_cb)
            m_on_error_cb(m_parent_client, message);
    }
    static void on_client_disconnected(uv_handle_t *handle) {
        auto a = (impl_old *)(handle->loop->data);
        a->stop_async();

        if (a->m_on_disconnected_cb != nullptr)
            a->m_on_disconnected_cb(a->m_parent_client);
    }
    static void alloc_cb(uv_handle_t *, size_t size, uv_buf_t *buf) {
        /* Generate buffer for the handle */
        *buf = uv_buf_init((char *)malloc(size), (unsigned int)size);
    }
    static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
        auto a = (impl_old *)(stream->data);

        /* Check for disconnection*/
        if (nread < 0) {
            free(buf->base);
            uv_close((uv_handle_t *)stream, on_client_disconnected);
            if (nread != UV_EOF)
                a->on_error(uv_strerror((int)nread));
            return;
        }

        /* Take ownership of the buffer, as it is now ours. */
        {
            std::lock_guard lock(a->m_read_data_mutex);
            auto b = sg::tcp_client_old::buffer_t((uint8_t *)buf->base, nread);
            a->data.emplace_back(std::move(b));
        }

        if (a->m_on_data_available != nullptr)
            a->m_on_data_available(a->m_parent_client, nread);
    }
    static void on_new_connection(uv_connect_t *connection, int status) {
        auto a = (impl_old *)(connection->data);

        if (status < 0) {
            a->on_error(uv_strerror((int)status));
            a->stop_async();
            return;
        }

        a->m_connected = true;

        if (a->m_on_connected_cb != nullptr)
            a->m_on_connected_cb(a->m_parent_client);

        uv_read_start(connection->handle, alloc_cb, on_read);
    }
    static void on_write(uv_write_s *req, int status) {
        auto a = (impl_old *)(req->data);

        if (status < 0)
            a->on_error( uv_strerror((int)status));

        std::lock_guard lock(a->m_write_data_mutex);
        a->m_write_data.erase(a->m_write_data.begin());
        a->m_write_requests.erase(a->m_write_requests.begin());
    }

protected:
    void setup_libuv_operations() override {
        /* Create address */
        struct sockaddr_in dest;
        THROW_ON_LIBUV_ERROR(uv_ip4_addr(m_address.c_str(), port, &dest));

        /* Create socket */
        m_sock = std::make_unique<uv_tcp_t>();
        THROW_ON_LIBUV_ERROR(uv_tcp_init(&m_loop, m_sock.get()));

        /* Enable TCP keep-alive */
        THROW_ON_LIBUV_ERROR(uv_tcp_keepalive(m_sock.get(), 1, 60));

        m_conn = std::make_unique<uv_connect_t>();
        m_conn->data = this;
        THROW_ON_LIBUV_ERROR(uv_tcp_connect(m_conn.get(), m_sock.get(), (const struct sockaddr *)&dest, on_new_connection));
    }
    void stop_libuv_operations() override {};


  public:
    impl_old(tcp_client_old *parent_client) :m_parent_client(parent_client) {
    }

    void connect(const int port,
               const std::string& address,
               on_error_cb_t on_error_cb,
               on_connected_cb_t on_connected_cb,
               on_disconnected_cb_t on_disconnected_cb,
               on_data_available_cb_t on_data_available_cb) {

        if (!is_stopped_or_stopping())
            throw std::logic_error("this tcp_client is currently running");

        /* if stopping, wait for it to stop */
        block_until_stopped();

        this->port = port;
        this->m_address = address;
        m_on_error_cb = on_error_cb;
        m_on_connected_cb = on_connected_cb;
        m_on_disconnected_cb = on_disconnected_cb;
        m_on_data_available = on_data_available_cb;

        auto on_libuv_stopped = [&](libuv_wrapper*){ m_connected = false;};

        start_libuv();
    }

    /* do not call this from libuv callbacks or loop */
    void disconnect() {
        stop();
    }

    void disconnect_async() {
        stop_async();
    }

    bool is_connected() const {
        return !is_stopped_or_stopping() && m_connected;
    }

    bool is_connected_or_connecting() const {
        return !is_stopped_or_stopping();
    }

    void write(std::vector<sg::tcp_client_old::buffer_t> vec) {
        std::lock_guard lock(m_write_data_mutex);

        /* store data, as it's needed for the callback */
        m_write_data.push_back(vec);

        /* create libuv buffers */
        std::vector<uv_buf_t> buffs;
        for (auto bytes: vec)
            buffs.push_back(uv_buf_init((char *)bytes.get(), bytes.size()));

        /* write request */
        m_write_requests.emplace_back(std::make_unique<uv_write_t>());
        auto req = m_write_requests.back().get();
        req->data = this;

        uv_write(req, (uv_stream_t *)m_sock.get(), buffs.data(), vec.size(), &on_write);
    }
};

tcp_client_old::tcp_client_old() : pimpl(sg::pimpl<impl_old>(this)) {}

void tcp_client_old::connect(const int port,
                       const std::string &address,
                       on_error_cb_t on_error_cb,
                       on_connected_cb_t on_connected_cb,
                       on_disconnected_cb_t on_disconnected_cb,
                       on_data_available_cb_t on_data_available_cb)
{
    pimpl->connect(port, address, on_error_cb, on_connected_cb, on_disconnected_cb, on_data_available_cb);
}

void tcp_client_old::disconnect()
{
    pimpl->disconnect();
}

bool tcp_client_old::is_connected() const
{
    return pimpl->is_connected();
}

bool tcp_client_old::is_connected_or_connecting() const
{
    return pimpl->is_connected_or_connecting();
}

void tcp_client_old::write(buffer_t data)
{
    std::vector<buffer_t> buffers;
    buffers.emplace_back(data);
    write(buffers);
}

void tcp_client_old::write(std::vector<buffer_t> buffers)
{
    pimpl->write(buffers);
}

tcp_client_old::~tcp_client_old() {
    pimpl->stop();
}

tcp_client::tcp_client():pimpl(sg::pimpl<impl>(this)) {}

void tcp_client::connect(const int port, const std::string &address)
{
    pimpl->connect(port, address);
}

tcp_client::~tcp_client() =default;

} // namespace sg
