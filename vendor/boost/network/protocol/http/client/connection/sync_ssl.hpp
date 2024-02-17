#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTPS_SYNC_CONNECTION_HTTP_20100601
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTPS_SYNC_CONNECTION_HTTP_20100601

// Copyright 2013 Google, Inc.
// Copyright 2010 (C) Dean Michael Berris <dberris@google.com>
// Copyright 2010 (C) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <functional>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/compat.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_connection_base_impl;

template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_connection_base;

template <class Tag, unsigned version_major, unsigned version_minor>
struct https_sync_connection
    : public virtual sync_connection_base<Tag, version_major, version_minor>,
      sync_connection_base_impl<Tag, version_major, version_minor>,
      std::enable_shared_from_this<
          https_sync_connection<Tag, version_major, version_minor> > {
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename string<Tag>::type string_type;
  typedef std::function<typename resolver_base::resolver_iterator_pair(
      resolver_type&, string_type const&, string_type const&)>
      resolver_function_type;
  typedef https_sync_connection<Tag, version_major, version_minor> this_type;
  typedef sync_connection_base_impl<Tag, version_major, version_minor>
      connection_base;
  typedef std::function<bool(string_type&)> body_generator_function_type;

  // FIXME make the certificate filename and verify path parameters be
  // optional ranges
  https_sync_connection(
      resolver_type& resolver, resolver_function_type resolve,
      bool always_verify_peer, int timeout,
      optional<string_type> /*unused*/ const& certificate_filename =
          optional<string_type>(),
      optional<string_type> const& verify_path = optional<string_type>(),
      optional<string_type> const& certificate_file = optional<string_type>(),
      optional<string_type> const& private_key_file = optional<string_type>(),
      optional<string_type> const& ciphers = optional<string_type>(),
      optional<string_type> const& sni_hostname = optional<string_type>(),
      long ssl_options = 0)
      : connection_base(),
        timeout_(timeout),
        timer_(CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver)),
        resolver_(resolver),
        resolve_(std::move(resolve)),
        context_(CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver), boost::asio::ssl::context::sslv23_client),
        socket_(CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver), context_) {
    if (ciphers) {
      ::SSL_CTX_set_cipher_list(context_.native_handle(), ciphers->c_str());
    }
    if (ssl_options != 0) {
      context_.set_options(ssl_options);
    }
    if (certificate_filename || verify_path) {
      context_.set_verify_mode(boost::asio::ssl::context::verify_peer);
      // FIXME make the certificate filename and verify path parameters
      // be
      // optional ranges
      if (certificate_filename)
        context_.load_verify_file(*certificate_filename);
      if (verify_path) context_.add_verify_path(*verify_path);
    } else {
      if (always_verify_peer)
        context_.set_verify_mode(boost::asio::ssl::context_base::verify_peer);
      else
        context_.set_verify_mode(boost::asio::ssl::context_base::verify_none);
    }
    if (certificate_file)
      context_.use_certificate_file(*certificate_file, boost::asio::ssl::context::pem);
    if (private_key_file)
      context_.use_private_key_file(*private_key_file, boost::asio::ssl::context::pem);
    if (sni_hostname)
      SSL_set_tlsext_host_name(socket_.native_handle(), sni_hostname->c_str());
  }

  void init_socket(string_type /*unused*/ const& hostname,
                   string_type const& port) {
    connection_base::init_socket(socket_.lowest_layer(), resolver_, hostname,
                                 port, resolve_);
    socket_.handshake(boost::asio::ssl::stream_base::client);
  }

  void send_request_impl(string_type /*unused*/ const& method,
                         basic_request<Tag> const& request_,
                         body_generator_function_type generator) {
    boost::asio::streambuf request_buffer;
    linearize(
        request_, method, version_major, version_minor,
        std::ostreambuf_iterator<typename char_<Tag>::type>(&request_buffer));
    connection_base::send_request_impl(socket_, method, request_buffer);
    if (generator) {
      string_type chunk;
      while (generator(chunk)) {
        std::copy(chunk.begin(), chunk.end(),
                  std::ostreambuf_iterator<typename char_<Tag>::type>(
                      &request_buffer));
        chunk.clear();
        connection_base::send_request_impl(socket_, method, request_buffer);
      }
    }
    if (timeout_ > 0) {
      timer_.expires_from_now(std::chrono::seconds(timeout_));
      auto self = this->shared_from_this();
      timer_.async_wait(
          [=](boost::system::error_code const& ec) { self->handle_timeout(ec); });
    }
  }

  void read_status(basic_response<Tag>& response_,
                   boost::asio::streambuf& response_buffer) {
    connection_base::read_status(socket_, response_, response_buffer);
  }

  void read_headers(basic_response<Tag>& response_,
                    boost::asio::streambuf& response_buffer) {
    connection_base::read_headers(socket_, response_, response_buffer);
  }

  void read_body(basic_response<Tag>& response_,
                 boost::asio::streambuf& response_buffer) {
    connection_base::read_body(socket_, response_, response_buffer);
    typename headers_range<basic_response<Tag> >::type connection_range =
        headers(response_)["Connection"];
    if (version_major == 1 && version_minor == 1 &&
        !boost::empty(connection_range) &&
        boost::iequals(std::begin(connection_range)->second, "close")) {
      close_socket();
    } else if (version_major == 1 && version_minor == 0) {
      close_socket();
    }
  }

  bool is_open() { return socket_.lowest_layer().is_open(); }

  void close_socket() {
    timer_.cancel();
    boost::system::error_code ignored;
    socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                    ignored);
    if (ignored) {
      return;
    }
    socket_.lowest_layer().close(ignored);
  }

  ~https_sync_connection() { close_socket(); }

 private:
  void handle_timeout(boost::system::error_code const& ec) {
    if (!ec) {
      close_socket();
    }
  }

  int timeout_;
  boost::asio::steady_timer timer_;
  resolver_type& resolver_;
  resolver_function_type resolve_;
  boost::asio::ssl::context context_;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTPS_SYNC_CONNECTION_HTTP_20100601
