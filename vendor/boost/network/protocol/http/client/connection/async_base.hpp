#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_ASYNC_CONNECTION_BASE_20100529
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_ASYNC_CONNECTION_BASE_20100529

// Copryight 2013 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Copyright 2010 (C) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <functional>
#include <array>
#include <boost/network/protocol/http/client/connection/connection_delegate_factory.hpp>
#include <boost/network/protocol/http/response.hpp>
#include <boost/network/protocol/http/traits/delegate_factory.hpp>
#include <boost/network/protocol/http/client/connection/async_normal.hpp>
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/network/compat.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

template <class Tag, unsigned version_major, unsigned version_minor>
struct async_connection_base {
  typedef async_connection_base<Tag, version_major, version_minor> this_type;
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename resolver_base::resolve_function resolve_function;
  typedef typename string<Tag>::type string_type;
  typedef basic_request<Tag> request;
  typedef basic_response<Tag> response;
  typedef
      typename std::array<typename char_<Tag>::type,
                          BOOST_NETWORK_HTTP_CLIENT_CONNECTION_BUFFER_SIZE>::
          const_iterator const_iterator;
  typedef iterator_range<const_iterator> char_const_range;
  typedef std::function<void(char_const_range const &, boost::system::error_code const &)>
      body_callback_function_type;
  typedef std::function<bool(string_type &)> body_generator_function_type;
  typedef std::shared_ptr<this_type> connection_ptr;

  // This is the factory function which constructs the appropriate async
  // connection implementation with the correct delegate chosen based on the
  // tag.
  static connection_ptr new_connection(
      resolve_function resolve, resolver_type &resolver, bool follow_redirect,
      bool always_verify_peer, bool https, int timeout,
      bool remove_chunk_markers,
      optional<string_type> certificate_filename = optional<string_type>(),
      optional<string_type> const &verify_path = optional<string_type>(),
      optional<string_type> certificate_file = optional<string_type>(),
      optional<string_type> private_key_file = optional<string_type>(),
      optional<string_type> ciphers = optional<string_type>(),
      optional<string_type> sni_hostname = optional<string_type>(),
      long ssl_options = 0) {
    typedef http_async_connection<Tag, version_major, version_minor>
        async_connection;
    typedef typename delegate_factory<Tag>::type delegate_factory_type;
    auto delegate = delegate_factory_type::new_connection_delegate(
        CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver), https, always_verify_peer,
        certificate_filename, verify_path, certificate_file, private_key_file,
        ciphers, sni_hostname, ssl_options);
    auto temp = std::make_shared<async_connection>(
        resolver, resolve, follow_redirect, timeout, remove_chunk_markers,
        std::move(delegate));
    BOOST_ASSERT(temp != nullptr);
    return temp;
  }

  // This is the pure virtual entry-point for all asynchronous
  // connections.
  virtual response start(request const &request, string_type const &method,
                         bool get_body, body_callback_function_type callback,
                         body_generator_function_type generator) = 0;

  virtual ~async_connection_base() = default;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_ASYNC_CONNECTION_BASE_20100529
