
// Copyright Dean Michael Berris 2007.
// Copyright Google, Inc. 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_DIRECTIVES_HPP__
#define BOOST_NETWORK_MESSAGE_DIRECTIVES_HPP__

#include <boost/network/message_fwd.hpp>
#include <boost/network/message/directives/detail/string_directive.hpp>
#include <boost/network/message/directives/header.hpp>
#include <boost/network/message/directives/remove_header.hpp>

namespace boost {
namespace network {

template <class Tag, class Directive>
inline basic_message<Tag>& operator<<(basic_message<Tag>& message_,
                                      Directive const& directive) {
  directive(message_);
  return message_;
}

BOOST_NETWORK_STRING_DIRECTIVE(source, source_, message.source(source_),
                               message.source = source_);
BOOST_NETWORK_STRING_DIRECTIVE(destination, destination_,
                               message.destination(destination_),
                               message.destination = destination_);
BOOST_NETWORK_STRING_DIRECTIVE(body, body_, message.body(body_),
                               message.body = body_);

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_DIRECTIVES_HPP__
