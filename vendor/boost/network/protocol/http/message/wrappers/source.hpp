#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_SOURCE_HPP_20100622
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_SOURCE_HPP_20100622

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/message/wrappers/helper.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

template <class Tag>
struct basic_request;

BOOST_NETWORK_DEFINE_HTTP_WRAPPER(source, source, source);

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_SOURCE_HPP_20100622
