#ifndef BOOST_NETWORK_MODIFIERS_BODY_HPP_20100824
#define BOOST_NETWORK_MODIFIERS_BODY_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/pod_or_normal.hpp>

namespace boost {
namespace network {

template <class Tag, template <class> class Message, class ValueType>
inline void body_impl(Message<Tag>& message, ValueType const& body, tags::pod /*unused*/ /*unused*/) {
  message.body = body;
}

template <class Tag, template <class> class Message, class ValueType>
inline void body_impl(Message<Tag>& message, ValueType const& body,
                      tags::normal /*unused*/ /*unused*/) {
  message.body(body);
}

template <class Tag, template <class> class Message, class ValueType>
inline void body(Message<Tag>& message, ValueType const& body_) {
  body_impl(message, body_, typename pod_or_normal<Tag>::type());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MODIFIERS_BODY_HPP_20100824
