#ifndef BOOST_NETWORK_PROTOCOL_HTTP_SERVER_SOCKET_OPTIONS_BASE_HPP_20101210
#define BOOST_NETWORK_PROTOCOL_HTTP_SERVER_SOCKET_OPTIONS_BASE_HPP_20101210

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/utility/in_place_factory.hpp>

namespace boost {
namespace network {
namespace http {

struct socket_options_base {
 protected:
  boost::asio::socket_base::reuse_address acceptor_reuse_address;
  boost::asio::socket_base::enable_connection_aborted acceptor_report_aborted;
  boost::optional<boost::asio::socket_base::receive_buffer_size> receive_buffer_size;
  boost::optional<boost::asio::socket_base::send_buffer_size> send_buffer_size;
  boost::optional<boost::asio::socket_base::receive_low_watermark>
      receive_low_watermark;
  boost::optional<boost::asio::socket_base::send_low_watermark> send_low_watermark;
  bool non_blocking_io;
  boost::asio::socket_base::linger linger;

  template <class Tag, class Handler>
  explicit socket_options_base(server_options<Tag, Handler> const &options)
      : acceptor_reuse_address(options.reuse_address()),
        acceptor_report_aborted(options.report_aborted()),
        receive_buffer_size(options.receive_buffer_size()),
        send_buffer_size(options.send_buffer_size()),
        receive_low_watermark(options.receive_low_watermark()),
        send_low_watermark(options.send_low_watermark()),
        non_blocking_io(options.non_blocking_io()),
        linger(options.linger(), options.linger_timeout()) {}

  void acceptor_options(boost::asio::ip::tcp::acceptor &acceptor) {
    acceptor.set_option(acceptor_reuse_address);
    acceptor.set_option(acceptor_report_aborted);
  }

  void socket_options(boost::asio::ip::tcp::socket &socket) {
    boost::system::error_code ignored;
    socket.non_blocking(non_blocking_io);
    socket.set_option(linger, ignored);
    if (receive_buffer_size) socket.set_option(*receive_buffer_size, ignored);
    if (receive_low_watermark)
      socket.set_option(*receive_low_watermark, ignored);
    if (send_buffer_size) socket.set_option(*send_buffer_size, ignored);
    if (send_low_watermark) socket.set_option(*send_low_watermark, ignored);
  }
};

} /* http */

} /* network */

} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_SERVER_SOCKET_OPTIONS_BASE_HPP_20101210 \
          */
