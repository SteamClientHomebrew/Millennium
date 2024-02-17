
#ifndef BOOST_NETWORK_MESSAGE_MODIFIER_DESTINATION_HPP_20100824
#define BOOST_NETWORK_MESSAGE_MODIFIER_DESTINATION_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {

namespace impl {

template <class Message, class ValueType, class Tag>
inline void destination(Message const &message, ValueType const &destination_,
                        Tag const & /*unused*/ /*unused*/, mpl::false_ const & /*unused*/ /*unused*/) {
  message.destination(destination_);
}

template <class Message, class ValueType, class Tag>
inline void destination(Message const &message, ValueType const &destination_,
                        Tag const & /*unused*/ /*unused*/, mpl::true_ const & /*unused*/ /*unused*/) {
  message.destination(destination_);
}
} // namespace impl // namespace impl

template <class Tag, template <class> class Message, class ValueType>
inline void destination(Message<Tag> const &message,
                        ValueType const &destination_) {
  impl::destination(message, destination_, Tag(), is_async<Tag>());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_MODIFIER_DESTINATION_HPP_20100824
