#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_IPP_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_IPP_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <functional>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/network/protocol/http/client/connection/normal_delegate.hpp>

boost::network::http::impl::normal_delegate::normal_delegate(
    boost::asio::io_service &service)
    : service_(service) {}

void boost::network::http::impl::normal_delegate::connect(
    boost::asio::ip::tcp::endpoint &endpoint, std::string host,
    std::uint16_t source_port, optional<std::string> sni_hostname,
    std::function<void(boost::system::error_code const &)> handler) {

  // TODO(dberris): review parameter necessity.
  (void)host;

  socket_.reset(new boost::asio::ip::tcp::socket(
      service_,
      boost::asio::ip::tcp::endpoint(boost::asio::ip::address(), source_port)));
  socket_->async_connect(endpoint, handler);
}

void boost::network::http::impl::normal_delegate::write(
    boost::asio::streambuf &command_streambuf,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  boost::asio::async_write(*socket_, command_streambuf, handler);
}

void boost::network::http::impl::normal_delegate::read_some(
    boost::asio::mutable_buffers_1 const &read_buffer,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  socket_->async_read_some(read_buffer, handler);
}

void boost::network::http::impl::normal_delegate::disconnect() {
  if (socket_.get() && socket_->is_open()) {
    boost::system::error_code ignored;
    socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    if (!ignored) {
      socket_->close(ignored);
    }
  }
}

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_NORMAL_DELEGATE_IPP_20110819
