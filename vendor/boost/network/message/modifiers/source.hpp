
#ifndef BOOST_NETWORK_MESSAGE_MODIFIER_SOURCE_HPP_20100824
#define BOOST_NETWORK_MESSAGE_MODIFIER_SOURCE_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {

namespace impl {

template <class Message, class ValueType, class Tag>
inline void source(Message const &message, ValueType const &source_,
                   Tag const & /*unused*/ /*unused*/, mpl::false_ const & /*unused*/ /*unused*/) {
  message.source(source_);
}

template <class Message, class ValueType, class Tag>
inline void source(Message const &message, ValueType const &source_,
                   Tag const & /*unused*/ /*unused*/, mpl::true_ const & /*unused*/ /*unused*/) {
  message.source(source_);
}

}  // namespace impl

template <class Tag, template <class> class Message, class ValueType>
inline void source(Message<Tag> const &message, ValueType const &source_) {
  impl::source(message, source_, Tag(), is_async<Tag>());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_MODIFIER_SOURCE_HPP_20100824
