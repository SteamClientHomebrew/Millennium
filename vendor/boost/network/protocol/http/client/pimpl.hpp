#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_PIMPL_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_PIMPL_HPP_20100623

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/not.hpp>
#include <boost/network/protocol/http/client/async_impl.hpp>
#include <boost/network/protocol/http/client/sync_impl.hpp>
#include <boost/network/protocol/http/traits/connection_policy.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_sync.hpp>
#include <boost/optional/optional.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, unsigned version_major, unsigned version_minor>
struct basic_client_impl;

namespace impl {

template <class Tag, unsigned version_major, unsigned version_minor>
struct async_client;

template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_client;

template <class Tag, unsigned version_major, unsigned version_minor,
          class Enable = void>
struct client_base {
  typedef unsupported_tag<Tag> type;
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct client_base<Tag, version_major, version_minor,
                   typename enable_if<is_async<Tag> >::type> {
  typedef async_client<Tag, version_major, version_minor> type;
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct client_base<Tag, version_major, version_minor,
                   typename enable_if<is_sync<Tag> >::type> {
  typedef sync_client<Tag, version_major, version_minor> type;
};

}  // namespace impl

template <class Tag, unsigned version_major, unsigned version_minor>
class basic_client;

template <class Tag, unsigned version_major, unsigned version_minor>
struct basic_client_impl
    : impl::client_base<Tag, version_major, version_minor>::type {
  static_assert(mpl::not_<mpl::and_<is_async<Tag>, is_sync<Tag> > >::value,
                "Client can only be synchronous our asynchronous.");

  typedef typename impl::client_base<Tag, version_major, version_minor>::type
      base_type;
  typedef typename base_type::string_type string_type;

  basic_client_impl(bool cache_resolved, bool follow_redirect,
                    bool always_verify_peer,
                    optional<string_type> const& certificate_filename,
                    optional<string_type> const& verify_path,
                    optional<string_type> const& certificate_file,
                    optional<string_type> const& private_key_file,
                    optional<string_type> const& ciphers,
                    optional<string_type> const& sni_hostname, long ssl_options,
                    std::shared_ptr<boost::asio::io_service> service, int timeout,
                    bool remove_chunk_markers)
      : base_type(cache_resolved, follow_redirect, always_verify_peer, timeout, 
                  remove_chunk_markers, service, certificate_filename, verify_path,
                  certificate_file, private_key_file, ciphers, sni_hostname,
                  ssl_options) {}

  ~basic_client_impl() = default;
};

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_PIMPL_HPP_20100623
