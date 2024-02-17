#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <functional>
#include <boost/asio/ssl.hpp>
#include <boost/network/protocol/http/client/connection/ssl_delegate.hpp>

boost::network::http::impl::ssl_delegate::ssl_delegate(
    boost::asio::io_service &service, bool always_verify_peer,
    optional<std::string> certificate_filename,
    optional<std::string> verify_path, optional<std::string> certificate_file,
    optional<std::string> private_key_file, optional<std::string> ciphers,
    optional<std::string> sni_hostname, long ssl_options)
    : service_(service),
      certificate_filename_(std::move(certificate_filename)),
      verify_path_(std::move(verify_path)),
      certificate_file_(std::move(certificate_file)),
      private_key_file_(std::move(private_key_file)),
      ciphers_(std::move(ciphers)),
      sni_hostname_(std::move(sni_hostname)),
      ssl_options_(ssl_options),
      always_verify_peer_(always_verify_peer) {}

void boost::network::http::impl::ssl_delegate::connect(
    boost::asio::ip::tcp::endpoint &endpoint, std::string host,
    std::uint16_t source_port, optional<std::string> sni_hostname,
    std::function<void(boost::system::error_code const &)> handler) {

  context_.reset(
      new boost::asio::ssl::context(boost::asio::ssl::context::method::sslv23_client));
  if (ciphers_) {
    ::SSL_CTX_set_cipher_list(context_->native_handle(), ciphers_->c_str());
  }
  if (ssl_options_ != 0) {
    context_->set_options(ssl_options_);
  } else {
    // By default, disable v3 support.
    context_->set_options(boost::asio::ssl::context::no_sslv3);
  }
  if (certificate_filename_ || verify_path_) {
    context_->set_verify_mode(boost::asio::ssl::context::verify_peer);
    if (certificate_filename_)
      context_->load_verify_file(*certificate_filename_);
    if (verify_path_) context_->add_verify_path(*verify_path_);
  } else {
    if (always_verify_peer_) {
      context_->set_verify_mode(boost::asio::ssl::context::verify_peer);
      // use openssl default verify paths.  uses openssl environment variables
      // SSL_CERT_DIR, SSL_CERT_FILE
      context_->set_default_verify_paths();
    } else {
      context_->set_verify_mode(boost::asio::ssl::context::verify_none);
    }
  }
  if (certificate_file_)
    context_->use_certificate_file(*certificate_file_, boost::asio::ssl::context::pem);
  if (private_key_file_)
    context_->use_private_key_file(*private_key_file_, boost::asio::ssl::context::pem);

  tcp_socket_.reset(new boost::asio::ip::tcp::socket(
      service_, boost::asio::ip::tcp::endpoint(endpoint.address().is_v4()
                                                   ? boost::asio::ip::tcp::v4()
                                                   : boost::asio::ip::tcp::v6(),
                                               source_port)));
  socket_.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>(
      *(tcp_socket_.get()), *context_));

  if (sni_hostname) { // at request level
    SSL_set_tlsext_host_name(socket_->native_handle(), sni_hostname->c_str());
  } else if (sni_hostname_) { // at client level
    SSL_set_tlsext_host_name(socket_->native_handle(), sni_hostname_->c_str());
  }


  if (always_verify_peer_)
    socket_->set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
  auto self = this->shared_from_this();
  socket_->lowest_layer().async_connect(
      endpoint,
      [=](boost::system::error_code const &ec) { self->handle_connected(ec, handler); });
}

void boost::network::http::impl::ssl_delegate::handle_connected(
    boost::system::error_code const &ec,
    std::function<void(boost::system::error_code const &)> handler) {
  if (!ec) {
    socket_->async_handshake(boost::asio::ssl::stream_base::client, handler);
  } else {
    handler(ec);
  }
}

void boost::network::http::impl::ssl_delegate::write(
    boost::asio::streambuf &command_streambuf,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  boost::asio::async_write(*socket_, command_streambuf, handler);
}

void boost::network::http::impl::ssl_delegate::read_some(
    boost::asio::mutable_buffers_1 const &read_buffer,
    std::function<void(boost::system::error_code const &, size_t)> handler) {
  socket_->async_read_some(read_buffer, handler);
}

void boost::network::http::impl::ssl_delegate::disconnect() {
  if (socket_.get() && socket_->lowest_layer().is_open()) {
    boost::system::error_code ignored;
    socket_->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                     ignored);
    if (!ignored) {
      socket_->lowest_layer().close(ignored);
    }
  }
}

boost::network::http::impl::ssl_delegate::~ssl_delegate() {}

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819
