#ifndef BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SIMPLE_CONNECTION_20091214
#define BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SIMPLE_CONNECTION_20091214

// Copyright 2013 Google, Inc.
// Copyright 2009 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iterator>
#include <memory>
#include <cstdint>
#include <functional>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/network/version.hpp>
#include <boost/network/protocol/http/client/connection/sync_base.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/optional/optional.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, unsigned version_major, unsigned version_minor>
struct simple_connection_policy : resolver_policy<Tag>::type {
 protected:
  typedef typename string<Tag>::type string_type;
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef std::function<typename resolver_base::resolver_iterator_pair(
      resolver_type&, string_type const&, string_type const&)>
      resolver_function_type;
  typedef typename resolver_base::resolver_completion_function
      resolver_completion_function;
  typedef std::function<void(iterator_range<char const*> const&,
                             boost::system::error_code const&)>
      body_callback_function_type;
  typedef std::function<bool(string_type&)> body_generator_function_type;

  struct connection_impl {
    connection_impl(
        resolver_type& resolver, bool follow_redirect, bool always_verify_peer,
        string_type /*unused*/ const& hostname, string_type const& port,
        resolver_function_type resolve, bool https, int timeout,
        optional<string_type> const& certificate_filename =
            optional<string_type>(),
        optional<string_type> const& verify_path = optional<string_type>(),
        optional<string_type> const& certificate_file = optional<string_type>(),
        optional<string_type> const& private_key_file = optional<string_type>(),
        optional<string_type> const& ciphers = optional<string_type>(),
        long ssl_options = 0)
        : pimpl(), follow_redirect_(follow_redirect) {
      // TODO(dberris): review parameter necessity.
      (void)hostname;
      (void)port;

      pimpl.reset(
          impl::sync_connection_base<Tag, version_major, version_minor>::
              new_connection(resolver, resolve, https, always_verify_peer,
                             timeout, certificate_filename, verify_path,
                             certificate_file, private_key_file, ciphers,
                             ssl_options));
    }

    basic_response<Tag> send_request(string_type /*unused*/ const& method,
                                     basic_request<Tag> request_, bool get_body,
                                     body_callback_function_type callback,
                                     body_generator_function_type generator) {
      // TODO(dberris): review parameter necessity.
      (void)callback;

      basic_response<Tag> response_;
      do {
        pimpl->init_socket(request_.host(), std::to_string(request_.port()));
        pimpl->send_request_impl(method, request_, generator);

        response_ = basic_response<Tag>();
        response_ << network::source(request_.host());

        boost::asio::streambuf response_buffer;
        pimpl->read_status(response_, response_buffer);
        pimpl->read_headers(response_, response_buffer);
        if (get_body) pimpl->read_body(response_, response_buffer);

        if (follow_redirect_) {
          std::uint16_t status = response_.status();
          if (status >= 300 && status <= 307) {
            typename headers_range<http::basic_response<Tag> >::type
                location_range = headers(response_)["Location"];
            typename range_iterator<
                typename headers_range<http::basic_response<Tag> >::type>::type
                location_header = std::begin(location_range);
            if (location_header != std::end(location_range)) {
              request_.uri(location_header->second);
            } else
              throw std::runtime_error(
                  "Location header not defined in redirect response.");
          } else {
            break;
          }
        } else {
          break;
        }
      } while (true);
      return response_;
    }

   private:
    std::shared_ptr<http::impl::sync_connection_base<Tag, version_major,
                                                     version_minor> > pimpl;
    bool follow_redirect_;
  };

  typedef std::shared_ptr<connection_impl> connection_ptr;
  connection_ptr get_connection(
      resolver_type& resolver, basic_request<Tag> const& request_,
      bool always_verify_peer,
      optional<string_type> /*unused*/ const& certificate_filename =
          optional<string_type>(),
      optional<string_type> const& verify_path = optional<string_type>(),
      optional<string_type> const& certificate_file = optional<string_type>(),
      optional<string_type> const& private_key_file = optional<string_type>(),
      optional<string_type> const& ciphers = optional<string_type>(),
      long ssl_options = 0) {
    namespace ph = std::placeholders;
    connection_ptr connection_(new connection_impl(
        resolver, follow_redirect_, always_verify_peer, request_.host(),
        std::to_string(request_.port()),
        [this](resolver_type& resolver, string_type const& host,
               std::uint16_t port, resolver_completion_function once_resolved) {
          this->resolve(resolver, host, port, once_resolved);
        },
        boost::iequals(request_.protocol(), string_type("https")), timeout_,
        certificate_filename, verify_path, certificate_file, private_key_file,
        ciphers, ssl_options));
    return connection_;
  }

  void cleanup() {}

  simple_connection_policy(bool cache_resolved, bool follow_redirect,
                           int timeout)
      : resolver_base(cache_resolved),
        follow_redirect_(follow_redirect),
        timeout_(timeout) {}

  // member variables
  bool follow_redirect_;
  int timeout_;
};

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SIMPLE_CONNECTION_20091214
