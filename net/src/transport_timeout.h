#pragma once

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>

namespace sg::net::detail {

/* Starts `start_op` with a completion token, gives up after `timeout_msec` (0 = wait
 * indefinitely), and returns the resulting error code. Shared by the transports. */
template <typename StartOp>
boost::asio::awaitable<boost::system::error_code> with_timeout(StartOp start_op,
                                                               unsigned timeout_msec) {
    /* SO_SNDTIMEO/SO_RCVTIMEO only apply to blocking calls, not async ones, hence cancel_after. */
    if (timeout_msec)
        co_return std::get<0>(co_await start_op(boost::asio::cancel_after(
            std::chrono::milliseconds(timeout_msec),
            boost::asio::as_tuple(boost::asio::use_awaitable))));

    co_return std::get<0>(co_await start_op(boost::asio::as_tuple(boost::asio::use_awaitable)));
}

} // namespace sg::net::detail
