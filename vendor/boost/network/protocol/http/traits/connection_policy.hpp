#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CONNECTION_POLICY_20091214
#define BOOST_NETWORK_PROTOCOL_HTTP_CONNECTION_POLICY_20091214

// Copyright 2013 Google, Inc.
// Copyright Dean Michael Berris 2009.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/policies/simple_connection.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/protocol/http/policies/pooled_connection.hpp>
#include <boost/network/protocol/http/policies/async_connection.hpp>
#include <boost/network/protocol/http/support/is_simple.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>
#include <boost/network/protocol/http/support/is_keepalive.hpp>
#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct unsupported_tag;

template <class Tag, unsigned version_major, unsigned version_minor,
          class Enable = void>
struct connection_policy {
  typedef unsupported_tag<Tag> type;
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct connection_policy<Tag, version_major, version_minor,
                         typename enable_if<is_async<Tag> >::type> {
  typedef async_connection_policy<Tag, version_major, version_minor> type;
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct connection_policy<
    Tag, version_major, version_minor,
    typename enable_if<
        mpl::and_<is_simple<Tag>, mpl::not_<is_async<Tag> > > >::type> {
  typedef simple_connection_policy<Tag, version_major, version_minor> type;
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct connection_policy<
    Tag, version_major, version_minor,
    typename enable_if<
        mpl::and_<is_keepalive<Tag>, mpl::not_<is_async<Tag> > > >::type> {
  typedef pooled_connection_policy<Tag, version_major, version_minor> type;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CONNECTION_POLICY_20091214
