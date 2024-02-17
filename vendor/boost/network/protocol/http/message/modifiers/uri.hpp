#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_URI_HPP_20100621
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_URI_HPP_20100621

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

template <class Tag, class T>
void uri(basic_request<Tag>& request, T const& value) {
  request.uri(value);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_URI_HPP_20100621
