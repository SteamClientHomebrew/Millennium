#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623

// Copyright 2013 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <functional>
#include <boost/network/protocol/http/client/options.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/protocol/http/traits/connection_policy.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/optional/optional.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, unsigned version_major, unsigned version_minor>
struct basic_client_impl;

namespace impl {
template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_client
    : connection_policy<Tag, version_major, version_minor>::type {
  typedef typename string<Tag>::type string_type;
  typedef typename connection_policy<Tag, version_major, version_minor>::type
      connection_base;
  typedef typename resolver<Tag>::type resolver_type;
  typedef std::function<void(iterator_range<char const*> const&,
                             boost::system::error_code const&)>
      body_callback_function_type;
  typedef std::function<bool(string_type&)> body_generator_function_type;
  friend struct basic_client_impl<Tag, version_major, version_minor>;

  std::shared_ptr<boost::asio::io_service> service_ptr;
  boost::asio::io_service& service_;
  resolver_type resolver_;
  optional<string_type> certificate_filename_;
  optional<string_type> verify_path_;
  optional<string_type> certificate_file_;
  optional<string_type> private_key_file_;
  optional<string_type> ciphers_;
  optional<string_type> sni_hostname_;
  long ssl_options_;
  bool always_verify_peer_;

  sync_client(
      bool cache_resolved, bool follow_redirect, bool always_verify_peer,
      int timeout, std::shared_ptr<boost::asio::io_service> service,
      optional<string_type> certificate_filename = optional<string_type>(),
      optional<string_type> verify_path = optional<string_type>(),
      optional<string_type> certificate_file = optional<string_type>(),
      optional<string_type> private_key_file = optional<string_type>(),
      optional<string_type> ciphers = optional<string_type>(),
      optional<string_type> sni_hostname = optional<string_type>(),
      long ssl_options = 0)
      : connection_base(cache_resolved, follow_redirect, timeout),
        service_ptr(service.get() ? service
                                  : std::make_shared<boost::asio::io_service>()),
        service_(*service_ptr),
        resolver_(service_),
        certificate_filename_(std::move(certificate_filename)),
        verify_path_(std::move(verify_path)),
        certificate_file_(std::move(certificate_file)),
        private_key_file_(std::move(private_key_file)),
        ciphers_(std::move(ciphers)),
        sni_hostname_(std::move(sni_hostname)),
        ssl_options_(ssl_options),
        always_verify_peer_(always_verify_peer) {}

  ~sync_client() = default;

  void wait_complete() {}

  basic_response<Tag> request_skeleton(basic_request<Tag> const& request_,
                                       string_type method, bool get_body,
                                       body_callback_function_type callback,
                                       body_generator_function_type generator) {
    typename connection_base::connection_ptr connection_;
    connection_ = connection_base::get_connection(
        resolver_, request_, always_verify_peer_, certificate_filename_,
        verify_path_, certificate_file_, private_key_file_, ciphers_,
        sni_hostname_);
    return connection_->send_request(method, request_, get_body, callback,
                                     generator);
  }
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623
