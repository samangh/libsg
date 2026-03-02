#pragma once

#include "asio_io_pool.h"
#include "buffer.h"
#include "debug.h"
#include "net.h"

#include <boost/asio/ip/tcp.hpp>
#include <sg/export/common.h>

namespace sg::net {

class SG_COMMON_EXPORT tcp_client_sync {
  public:
    tcp_client_sync();
    explicit tcp_client_sync(std::shared_ptr<asio_io_pool> context);
    virtual ~tcp_client_sync();

    void connect(const end_point& endpoint);
    void disconnect();

    [[nodiscard]] bool is_connected() const;

    [[nodiscard]] std::string read_until(std::string_view delimiter);
    [[nodiscard]] std::string read_some(size_t size);

    void write(const std::byte* data, size_t length);
    void write(const shared_c_buffer<std::byte>& msg);
    void write(std::string_view data);

  private:
    std::shared_ptr<asio_io_pool> m_context;
    boost::asio::ip::tcp::socket m_socket;

    /* needed because the asio::read_until can read past the delimiter! */
    std::string m_read_leftover{};
};

}
