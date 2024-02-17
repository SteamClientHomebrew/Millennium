#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_VERSION_HPP_20100603
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_VERSION_HPP_20100603

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/message/directives/detail/string_directive.hpp>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

BOOST_NETWORK_STRING_DIRECTIVE(version, version_, message.version(version_),
                               message.version = version_);

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_VERSION_HPP_20100603
