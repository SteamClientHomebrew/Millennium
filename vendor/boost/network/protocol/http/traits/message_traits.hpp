
// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Some changes Copyright (c) Dean Michael Berris 2008

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HPP

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct delimiters;

template <class Tag>
struct headers_;

template <class Tag>
struct content;

template <class Tag>
struct request_methods;

template <class Tag>
struct response_message;

template <class Tag>
struct response_code;

template <class Tag>
struct query_container;

template <class Tag>
struct cookies_container;

template <class Tag>
struct chunk_cache;

}  // namespace http

}  // namespace network

}  // namespace boost

// Defer definition in implementation files
#include <boost/network/protocol/http/traits/impl/delimiters.ipp>
#include <boost/network/protocol/http/traits/impl/headers.ipp>
#include <boost/network/protocol/http/traits/impl/content.ipp>
#include <boost/network/protocol/http/traits/impl/request_methods.ipp>
#include <boost/network/protocol/http/traits/impl/response_message.ipp>
#include <boost/network/protocol/http/traits/impl/response_code.ipp>
#include <boost/network/protocol/http/traits/impl/headers_container.ipp>
#include <boost/network/protocol/http/traits/impl/query_container.ipp>
#include <boost/network/protocol/http/traits/impl/cookies_container.ipp>
#include <boost/network/protocol/http/traits/impl/chunk_cache.ipp>

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_HPP
