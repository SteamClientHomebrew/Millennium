// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Some changes Copyright 2008 (c) Dean Michael Berris

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_PARSER_TRAITS_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_PARSER_TRAITS_HPP

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct status_message_text;

template <class Tag>
struct method;

template <class Tag>
struct resource;

template <class Tag>
struct query_string;

template <class Tag>
struct header_name;

template <class Tag>
struct header_value;

template <class Tag>
struct query_name;

template <class Tag>
struct query_value;

template <class Tag>
struct cookie_name;

template <class Tag>
struct cookie_value;

template <class Tag>
struct post_content;

}  // namespace http

}  // namespace network

}  // namespace boost

// Include implementation files
#include <boost/network/protocol/http/traits/impl/status_message.ipp>
#include <boost/network/protocol/http/traits/impl/method.ipp>
#include <boost/network/protocol/http/traits/impl/resource.ipp>
#include <boost/network/protocol/http/traits/impl/query_string.ipp>
#include <boost/network/protocol/http/traits/impl/header_name.ipp>
#include <boost/network/protocol/http/traits/impl/header_value.ipp>
#include <boost/network/protocol/http/traits/impl/query_name.ipp>
#include <boost/network/protocol/http/traits/impl/query_value.ipp>
#include <boost/network/protocol/http/traits/impl/cookie_name.ipp>
#include <boost/network/protocol/http/traits/impl/cookie_value.ipp>
#include <boost/network/protocol/http/traits/impl/post_content.ipp>

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_PARSER_TRAITS_HPP
