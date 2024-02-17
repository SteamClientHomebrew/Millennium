#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_CONNECTION_DELEGATE_HPP_
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_CONNECTION_DELEGATE_HPP_

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <functional>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

struct connection_delegate {
  virtual void connect(boost::asio::ip::tcp::endpoint &endpoint, std::string host,
                       std::uint16_t source_port, optional<std::string> sni_hostname,
                       std::function<void(boost::system::error_code const &)> handler) = 0;
  virtual void write(
      boost::asio::streambuf &command_streambuf,
      std::function<void(boost::system::error_code const &, size_t)> handler) = 0;
  virtual void read_some(
      boost::asio::mutable_buffers_1 const &read_buffer,
      std::function<void(boost::system::error_code const &, size_t)> handler) = 0;
  virtual void disconnect() = 0;
  virtual ~connection_delegate() = default;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_CONNECTION_DELEGATE_HPP_ \
          */
