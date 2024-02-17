#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <cstdint>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/network/protocol/http/client/connection/connection_delegate.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

struct normal_delegate : connection_delegate {
  explicit normal_delegate(boost::asio::io_service &service);

  void connect(boost::asio::ip::tcp::endpoint &endpoint, std::string host,
               std::uint16_t source_port, optional<std::string> sni_hostname,
               std::function<void(boost::system::error_code const &)> handler) override;
  void write(boost::asio::streambuf &command_streambuf,
             std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  void read_some(boost::asio::mutable_buffers_1 const &read_buffer,
                 std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  void disconnect() override;
  ~normal_delegate() override = default;

  normal_delegate(normal_delegate const &) = delete;
  normal_delegate &operator=(normal_delegate) = delete;

 private:
  boost::asio::io_service &service_;
  std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_20110819
