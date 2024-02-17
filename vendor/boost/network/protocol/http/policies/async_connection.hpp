#ifndef BOOST_NETWORK_POLICY_ASYNC_CONNECTION_HPP_20100529
#define BOOST_NETWORK_POLICY_ASYNC_CONNECTION_HPP_20100529

// Copyright 2010 (C) Dean Michael Berris
// Copyright 2010 (C) Sinefunc, Inc.
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <functional>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/network/protocol/http/client/connection/async_base.hpp>
#include <boost/network/protocol/http/message/wrappers/protocol.hpp>
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/version.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, unsigned version_major, unsigned version_minor>
struct async_connection_policy : resolver_policy<Tag>::type {
 protected:
  typedef typename string<Tag>::type string_type;
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename resolver_base::resolve_function resolve_function;
  typedef typename resolver_base::resolve_completion_function
      resolve_completion_function;
  typedef
      typename std::array<typename char_<Tag>::type,
                          BOOST_NETWORK_HTTP_CLIENT_CONNECTION_BUFFER_SIZE>::
          const_iterator const_iterator;
  typedef iterator_range<const_iterator> char_const_range;
  typedef std::function<void(char_const_range,
                             boost::system::error_code const&)>
      body_callback_function_type;
  typedef std::function<bool(string_type&)> body_generator_function_type;

  struct connection_impl {
    connection_impl(
        bool follow_redirect, bool always_verify_peer, resolve_function resolve,
        resolver_type& resolver, bool https, int timeout, bool remove_chunk_markers,
        optional<string_type> /*unused*/ const& certificate_filename,
        optional<string_type> const& verify_path,
        optional<string_type> const& certificate_file,
        optional<string_type> const& private_key_file,
        optional<string_type> const& ciphers,
        optional<string_type> const& sni_hostname, long ssl_options) {
      pimpl = impl::async_connection_base<Tag, version_major, version_minor>::
          new_connection(resolve, resolver, follow_redirect, always_verify_peer,
                         https, timeout, remove_chunk_markers,
                         certificate_filename, verify_path, certificate_file,
                         private_key_file, ciphers, sni_hostname, ssl_options);
    }

    basic_response<Tag> send_request(string_type /*unused*/ const& method,
                                     basic_request<Tag> const& request_,
                                     bool get_body,
                                     body_callback_function_type callback,
                                     body_generator_function_type generator) {
      return pimpl->start(request_, method, get_body, callback, generator);
    }

   private:
    std::shared_ptr<http::impl::async_connection_base<Tag, version_major,
                                                      version_minor> > pimpl;
  };

  typedef std::shared_ptr<connection_impl> connection_ptr;
  connection_ptr get_connection(
      resolver_type& resolver, basic_request<Tag> const& request_,
      bool always_verify_peer,
      optional<string_type> const& certificate_filename =
          optional<string_type>(),
      optional<string_type> const& verify_path = optional<string_type>(),
      optional<string_type> const& certificate_file = optional<string_type>(),
      optional<string_type> const& private_key_file = optional<string_type>(),
      optional<string_type> const& ciphers = optional<string_type>(),
      optional<string_type> const& sni_hostname = optional<string_type>(),
      long ssl_options = 0) {
    string_type protocol_ = protocol(request_);
    namespace ph = std::placeholders;
    return std::make_shared<connection_impl>(
        follow_redirect_, always_verify_peer,
        [this](resolver_type& resolver, string_type const& host,
               std::uint16_t port, resolve_completion_function once_resolved) {
          this->resolve(resolver, host, port, once_resolved);
        },
        resolver, boost::iequals(protocol_, string_type("https")), timeout_,
        remove_chunk_markers_, certificate_filename, verify_path,
        certificate_file, private_key_file, ciphers, sni_hostname, ssl_options);
  }

  void cleanup() {}

  async_connection_policy(bool cache_resolved, bool follow_redirect,
                          int timeout, bool remove_chunk_markers)
      : resolver_base(cache_resolved),
        follow_redirect_(follow_redirect),
        timeout_(timeout),
        remove_chunk_markers_(remove_chunk_markers) {}

  bool follow_redirect_;
  int timeout_;
  bool remove_chunk_markers_;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_POLICY_ASYNC_CONNECTION_HPP_
