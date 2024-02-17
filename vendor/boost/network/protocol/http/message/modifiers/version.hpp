#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_VERSION_HPP_20100608
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_VERSION_HPP_20100608

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/if.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_sync.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

namespace impl {

template <class Tag, class T>
void version(basic_response<Tag> &response, T const &value,
             mpl::false_ const & /*unused*/) {
  response << boost::network::http::version(value);
}

template <class Tag, class T>
void version(basic_response<Tag> &response, T const &future,
             mpl::true_ const & /*unused*/) {
  response.version(future);
}

}  // namespace impl

template <class Tag, class T>
void version(basic_response<Tag> &response, T const &value) {
  impl::version(response, value, is_async<Tag>());
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_VERSION_HPP_20100608
