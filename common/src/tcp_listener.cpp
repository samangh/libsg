#include "sg/tcp_listener.h"
#include "sg/buffer.h"
#include "sg/libuv_wrapper.h"

#include <cstring>
#include <future>
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

#ifdef _WIN32
    #include <WinSock2.h>
    #include <Ws2def.h>
#else
#include <sys/socket.h>
#endif

namespace {
struct result {
    bool success;
    std::string error_message;
};
} // namespace

namespace sg {

class SG_COMMON_EXPORT tcp_listener::impl : public sg::enable_lifetime_indicator {
 public:
    impl(tcp_listener& parent_listener)
       :m_parent_listener(parent_listener) {}

    ~impl() {
        stop();
    }

   void start(const std::string& address,
              int port,
              sg::tcp_listener::on_error_fn on_error_cb,
              sg::tcp_listener::on_client_connected_fn on_client_connected_cb,
              sg::tcp_listener::on_client_disconnected_fn on_client_disconnected_cb,
              sg::tcp_listener::on_started_fn on_started_listening_cb,
              sg::tcp_listener::on_stopped_fn on_stopped_listening_cb,
              sg::tcp_listener::on_data_available_fn on_data_available_cb) {
       if (!is_stopped_or_stopping())
           throw std::logic_error("this tcp_listener is currently running");

       block_until_stopped();

       this->port = port;

       m_on_error_cb = on_error_cb;
       m_on_client_connected_cb = on_client_connected_cb;
       m_on_client_disconnected_cb = on_client_disconnected_cb;
       m_on_data_available = on_data_available_cb;
       m_on_stopped_listening_cb = on_stopped_listening_cb;
       m_on_started_listening_cb  = on_started_listening_cb;

       // Clean previous use data
       {
           std::lock_guard lock(m_mutex);
           m_client_counter = 0;
           m_clients.clear();
       }

       /* for holding result of listening attempt*/
       std::promise<result> conn_promise;

       std::function<void(sg::libuv_wrapper *)> setup_func = [&, this](sg::libuv_wrapper *wrap) {
           try {
               /* Create address */
               void* dest;
               struct sockaddr_in dest4;
               struct sockaddr_in6 dest6;

               /* Create socket */
               m_sock = std::make_unique<uv_tcp_t>();
               m_sock->data = this;

               /* Check if IPv6 or IPv4 address */
               if (uv_ip4_addr(address.c_str(), port, &dest4) == 0)
                   dest=&dest4;
               else if (uv_ip6_addr(address.c_str(), port, &dest6) == 0)
                   dest=&dest6;
               else
                   throw std::invalid_argument("provided address is neither an IPv4 or IPv6 address");

               THROW_ON_LIBUV_ERROR(uv_tcp_init(wrap->get_uv_loop(), m_sock.get()));
               THROW_ON_LIBUV_ERROR(uv_tcp_bind(m_sock.get(), (const struct sockaddr *)dest, 0));
               THROW_ON_LIBUV_ERROR(uv_listen((uv_stream_t *)m_sock.get(), 20, on_new_connection));

               result r{true, ""};
               conn_promise.set_value(r);
           } catch (const std::exception& e) {
               result r{false, e.what()};
               conn_promise.set_value(r);
           }
       };

       std::function<void(sg::libuv_wrapper *)> started_func = [](sg::libuv_wrapper *) {

       };

       std::function<void(sg::libuv_wrapper *)> stopped_func = [&, this](sg::libuv_wrapper *) {
           if (m_on_stopped_listening_cb)
               m_on_stopped_listening_cb(m_parent_listener);
       };

       m_libuv.add_on_loop_started_cb(started_func);
       m_libuv.add_on_stopped_cb(stopped_func);

       m_libuv.start_task( setup_func, nullptr);

       auto listen_result = conn_promise.get_future().get();
       if (listen_result.success) {
           if (m_on_started_listening_cb)
               m_on_started_listening_cb(m_parent_listener);
       } else
       {
           stop();
           throw std::runtime_error(listen_result.error_message);
       }
   }

   void disconnect_async(client_id id) {
       std::shared_lock lock(m_mutex);
       uv_close((uv_handle_t*)m_clients.at(id)->uv_tcp_handle.get(), nullptr);
   }


   void block_until_stopped() {
       m_libuv.block_until_stopped();
   }

   bool is_stopped() const {
       return m_libuv.is_stopped();
   }

   bool is_stopped_or_stopping() const {
       return m_libuv.is_stopped_or_stopping();
   }

   void stop_async() {
       m_libuv.stop_async();
   }

   void stop(){
       m_libuv.stop();
   }

   size_t number_of_clients() const;
   void write(client_id, buffer&& data);

   connection_details client_connection_details(client_id id) const {
       std::shared_lock lock(m_mutex);
       auto client =  &m_clients.at(id);

       std::shared_lock lock2(client->get()->mutex);
       return client->get()->client_connection_details;
   }

   void keepalive_enable(bool en) {
       m_keepalive_emabled = en;
   }
   void keepalive_delay(unsigned int delay) {
       m_keepalive_delay = delay;
   }

 private:
   tcp_listener& m_parent_listener;
   int port;

   sg::libuv_wrapper m_libuv;

   struct client_data;
   struct write_request {
       write_request(client_data& _client,
                     write_req_id _write_id,
                     buffer&& data)
           : write_id(_write_id),
             data(std::move(data)),
             client_data_ptr(_client),
             uv_write_req_handle(std::make_unique<uv_write_t>()) {
           uv_write_req_handle->data = this;
       }
       write_req_id write_id;
       buffer data;
       client_data& client_data_ptr;
       std::unique_ptr<uv_write_t> uv_write_req_handle;
   };

   struct client_data {
       client_data(impl *_listener, client_id _id)
           : listener(_listener),
             id(_id),                                    //
             uv_tcp_handle(std::make_unique<uv_tcp_t>()) //
       {
           uv_tcp_handle->data = this;
       }
       mutable std::shared_mutex mutex;

       impl *listener;
       client_id id;
       std::unique_ptr<uv_tcp_t> uv_tcp_handle;
       connection_details client_connection_details;

       write_req_id m_write_request_counter;
       std::map<write_req_id, std::unique_ptr<write_request>> m_write_requests;

       void write(buffer&& data) {
           uv_buf_t wrbuf = uv_buf_init((char *)data.get(),
                                        static_cast<unsigned int>(data.size()));

           std::unique_lock lock(mutex);
           auto req_number = m_write_request_counter++;

           auto req = std::make_unique<write_request>(*this, req_number, std::move(data));
           auto uv_write_req_ = req->uv_write_req_handle.get();
           auto uv_stream_ = (uv_stream_t *)req->client_data_ptr.uv_tcp_handle.get();

           uv_write_req_->data = req.get();
           m_write_requests.emplace(req_number, std::move(req));
           uv_write(uv_write_req_, uv_stream_, &wrbuf, 1, &on_write);

       }

       void remove_write_request(write_req_id id) {
          std::unique_lock lock(mutex);
          m_write_requests.erase(id);
       }
   };


   void remove_write_request(write_req_id);

   void populate_client_details(std::shared_ptr<client_data> client);

   /* libuv callbacks */
   static void on_read(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf);
   static void on_new_connection(uv_stream_t *stream, int status);
   static void alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);
   static void on_client_disconnected(uv_handle_t *handle);
   static void on_write(uv_write_s *req, int status);
   void on_error(client_id, const std::string &message);

   /* call backs to user */
   sg::tcp_listener::on_error_fn m_on_error_cb; /* Called in case of errors after connect(...) */
   sg::tcp_listener::on_client_connected_fn m_on_client_connected_cb;
   sg::tcp_listener::on_client_disconnected_fn m_on_client_disconnected_cb;
   sg::tcp_listener::on_data_available_fn m_on_data_available;
   sg::tcp_listener::on_started_fn m_on_started_listening_cb;
   sg::tcp_listener::on_stopped_fn m_on_stopped_listening_cb;

   /* clients*/
   mutable std::shared_mutex m_mutex; /* Mutex for m_clients*/
   size_t m_client_counter;
   std::map<client_id, std::shared_ptr<client_data>> m_clients;

   /* write requets*/
   mutable std::shared_mutex m_write_req_mutex; /* Mutex for writing data*/


   /* libuv stuff*/
   std::unique_ptr<uv_tcp_t> m_sock;     /* Socket used for connection */

   /* keepalive stuff */
   bool m_keepalive_emabled {true};
   unsigned int m_keepalive_delay {1};
};

void close_handle(uv_handle_s *handle, void *) {
    uv_close(handle, nullptr);
}


size_t tcp_listener::impl::number_of_clients() const {
   std::shared_lock lock(m_mutex);
   return m_clients.size();
}

void tcp_listener::impl::write(client_id id, buffer&& data) {
    std::shared_lock lock(m_mutex);
    m_clients.at(id)->write(std::move(data));
}

void tcp_listener::impl::on_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
   auto client_dat  = (client_data *)client->data;
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

   {
       std::unique_lock lock(a->m_mutex);
       client_dat->client_connection_details.bytes_received += nread;
   }

   /* Take ownership of the buffer, as it is now ours. */
   auto buffer = sg::tcp_listener::buffer(reinterpret_cast<std::byte *>(buf->base), nread);
   if (a->m_on_data_available != nullptr)
       a->m_on_data_available(a->m_parent_listener, id, std::move(buffer));
}

void tcp_listener::impl::on_new_connection(uv_stream_t *server, int status) {
   auto a = (impl *)(server->data);
   if (status < 0) {
       a->on_error(0, uv_strerror((int)status));
       return;
   }

   client_id id;
   std::shared_ptr<client_data> client;
   {
       std::unique_lock lock(a->m_mutex);

       /* increment client id, check that it's not in use (in case of overflow) */
       while(true)
       {
           id = a->m_client_counter++;
           if  (!a->m_clients.contains(id))
               break;
       }
       a->m_clients.emplace(id, std::make_shared<client_data>(a, id));

       client = a->m_clients.at(id);
   }

   uv_tcp_t * tcp_handle = a->m_clients.at(id)->uv_tcp_handle.get();
   THROW_ON_LIBUV_ERROR(uv_tcp_init(server->loop, tcp_handle));

   /* uv_acceppt is guaraneed to succeeed */
   uv_accept(server, (uv_stream_t *)tcp_handle);

   if (a->m_keepalive_emabled)
    THROW_ON_LIBUV_ERROR(uv_tcp_keepalive(tcp_handle, 1, a->m_keepalive_delay));

   /* store source address, etc */
   a->populate_client_details(client);

   /* call this after accepting, in case the CB tries to write/close the cb */
   if (a->m_on_client_connected_cb != nullptr)
       a->m_on_client_connected_cb(a->m_parent_listener, id);

    uv_read_start((uv_stream_t *)tcp_handle, alloc_cb, on_read);
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
       std::unique_lock lock(a->m_mutex);
       a->m_clients.erase(id);
   }
}

void tcp_listener::impl::on_write(uv_write_s *req, int status) {
   auto write_req  = ((write_request *)req->data);
   auto& client = write_req->client_data_ptr;

   auto listener = client.listener;

   if (status < 0)
       listener->on_error(client.id, uv_strerror((int)status));
   else {
       std::lock_guard lock(client.mutex);
       client.client_connection_details.bytes_sent += write_req->data.size();
   }
   client.remove_write_request(write_req->write_id);
}

void tcp_listener::impl::populate_client_details(std::shared_ptr<client_data> client)
{
    struct sockaddr_storage addr;
    char ip_str[INET6_ADDRSTRLEN];

    auto tcp_handle = client->uv_tcp_handle.get();

    /* get client address */
    {
        int addr_length = sizeof(addr);

        /* get scokaddr structure */
        memset(&addr, 0, addr_length);
        THROW_ON_LIBUV_ERROR(uv_tcp_getpeername(tcp_handle, (struct sockaddr *)&addr, &addr_length));

        /* convert binary IP details to string */
        THROW_ON_LIBUV_ERROR(uv_ip_name((struct sockaddr *)&addr, ip_str, sizeof(ip_str)));
        client->client_connection_details.source_address = ip_str;

        if (addr.ss_family == AF_INET)
            client->client_connection_details.address_family = sg::net::address_family::IPv4;
        else if (addr.ss_family == AF_INET6)
            client->client_connection_details.address_family = sg::net::address_family::IPv6;
    }

    /* get socket bound address */
    {
        /* reset back to full value */
        int addr_length = sizeof(addr);

        /* get scokaddr structure */
        memset(&addr, 0, addr_length);
        THROW_ON_LIBUV_ERROR(uv_tcp_getsockname(tcp_handle, (struct sockaddr *)&addr, &addr_length));

        /* convert binary IP details to string */
        THROW_ON_LIBUV_ERROR(uv_ip_name((struct sockaddr *)&addr, ip_str, sizeof(ip_str)));
        client->client_connection_details.local_address = ip_str;
    }

    client->client_connection_details.bytes_sent=0;
    client->client_connection_details.bytes_received=0;
}


/************************************************************
*  PUBLIC INTERFACE
***********************************************************/

void tcp_listener::impl::on_error(client_id id, const std::string &message) {
   if (m_on_error_cb != nullptr)
       m_on_error_cb(m_parent_listener, id, message);
}

tcp_listener::tcp_listener() : pimpl(*this) {}

tcp_listener::~tcp_listener()  = default;


void tcp_listener::start(const std::string& address,
                        int port,
                        on_error_fn on_error_cb,
                        on_client_connected_fn on_client_connected_cb,
                        on_client_disconnected_fn on_client_disconnected_cb,
                        on_started_fn on_start,
                        on_stopped_fn on_stop,
                        on_data_available_fn on_data_available_cb) {
   pimpl->start(address,
                port,
                on_error_cb,
                on_client_connected_cb,
                on_client_disconnected_cb,
                on_start,
                on_stop,
                 on_data_available_cb);
}

void tcp_listener::stop_async() { pimpl->stop_async(); }

void tcp_listener::stop() { pimpl->stop(); }

void tcp_listener::disconnect_async(client_id id)
{
    pimpl->disconnect_async(id);
}

bool tcp_listener::is_stopped() const
{
    return pimpl->is_stopped();
}

bool tcp_listener::is_stopped_or_stopping() const { return pimpl->is_stopped_or_stopping(); }

size_t tcp_listener::number_of_clients() const { return pimpl->number_of_clients(); }

tcp_listener::connection_details tcp_listener::client_details(client_id id) const
{
    return pimpl->client_connection_details(id);
}


void tcp_listener::write(client_id id, buffer&& bytes) {
    pimpl->write(id, std::move(bytes));
}


void tcp_listener::write(client_id id, const void *ptr, size_t size) {
    auto buf = sg::make_unique_c_buffer<std::byte>(size);
    std::memcpy(buf.get(), ptr, size);
    write(id, std::move(buf));
}

void tcp_listener::keepalive_enable(bool en)
{
    pimpl->keepalive_enable(en);
}

void tcp_listener::keepalive_delay(unsigned int delay)
{
    pimpl->keepalive_delay(delay);
}


std::vector<std::byte> tcp_listener::buffers_to_vector(std::vector<buffer> buffers) {
   size_t total_size = 0;
   for (const auto &buf : buffers)
       total_size += buf.size();

   std::vector<std::byte> result;
   result.reserve(total_size);

   for (auto &buf : buffers)
       result.insert(result.end(), buf.begin(), buf.end());

   return result;
}


} // namespace sg
