#pragma once

#include "sg/buffer.h"
#include "sg/export/common.h"
#include "sg/pimpl.h"
#include "sg/net.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT tcp_listener : sg::enable_lifetime_indicator {
   class impl;
   sg::pimpl<impl> pimpl;

 public:
   typedef size_t client_id;
   typedef size_t write_req_id;
   typedef sg::unique_c_buffer<std::byte> buffer;

   typedef std::function<void(tcp_listener &, client_id, const std::string &msg)> on_error_fn;
   typedef std::function<void(tcp_listener &, client_id)> on_client_connected_fn;
   typedef std::function<void(tcp_listener &, client_id)> on_client_disconnected_fn;
   typedef std::function<void(tcp_listener &)> on_started_fn;
   typedef std::function<void(tcp_listener &)> on_stopped_fn;
   typedef std::function<void(tcp_listener &, client_id, buffer data)> on_data_available_fn;

   struct connection_details {
       std::string source_address;
       std::string local_address;
       sg::net::address_family address_family;
       size_t bytes_sent;
       size_t bytes_received;
   };

   /* Consructs a TCP listener, callbacks can be nullptr */
   tcp_listener();
   ~tcp_listener();

   void start(const std::string& address,
              int port,
              on_error_fn on_error_cb,
              on_client_connected_fn on_client_connected_cb,
              on_client_disconnected_fn on_client_disconnected_cb,
              on_started_fn on_start,
              on_stopped_fn on_stop,
              on_data_available_fn on_data_available_cb);
   void stop_async();
   void stop();

   void disconnect_async(client_id);

   bool is_stopped() const;
   bool is_stopped_or_stopping() const;

   size_t number_of_clients() const;

   connection_details client_details(client_id) const;

   void write(client_id, buffer&&);
   void write(client_id, const std::vector<std::byte>&);

   static std::vector<std::byte> buffers_to_vector(std::vector<buffer>);
};

} // namespace sg
