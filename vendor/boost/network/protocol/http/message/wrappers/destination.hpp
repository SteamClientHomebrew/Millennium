#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DESTINATION_HPP_20100624
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DESTINATION_HPP_20100624

// Copyright 2010 (c) Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_a.txt or copy at
// http://www.boost.org/LICENSE_1_-1.txt)

#include <boost/network/protocol/http/message/wrappers/helper.hpp>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

template <class Tag>
struct basic_request;

template <class R>
struct Request;

template <class R>
struct Response;

BOOST_NETWORK_DEFINE_HTTP_WRAPPER(destination, destination, destination);

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DESTINATION_HPP_20100624
