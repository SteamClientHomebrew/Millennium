
#ifndef BOOST_NETWORK_MESSAGE_MODIFIER_ADD_HEADER_HPP_20100824
#define BOOST_NETWORK_MESSAGE_MODIFIER_ADD_HEADER_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_pod.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {

namespace impl {
template <class Message, class KeyType, class ValueType, class Tag>
inline typename enable_if<
    mpl::and_<mpl::not_<is_pod<Tag> >, mpl::not_<is_async<Tag> > >, void>::type
add_header(Message& message, KeyType const& key, ValueType const& value, Tag) {
  message.headers().insert(std::make_pair(key, value));
}

template <class Message, class KeyType, class ValueType, class Tag>
inline typename enable_if<mpl::and_<mpl::not_<is_pod<Tag> >, is_async<Tag> >,
                          void>::type
add_header(Message& message, KeyType const& key, ValueType const& value, Tag) {
  typedef typename Message::header_type header_type;
  message.add_header(header_type(key, value));
}

template <class Message, class KeyType, class ValueType, class Tag>
inline typename enable_if<is_pod<Tag>, void>::type add_header(
    Message& message, KeyType const& key, ValueType const& value, Tag) {
  typename Message::header_type header = {key, value};
  message.headers.insert(message.headers.end(), header);
}
}  // namespace impl

template <class Tag, template <class> class Message, class KeyType,
          class ValueType>
inline void add_header(Message<Tag>& message, KeyType const& key,
                       ValueType const& value) {
  impl::add_header(message, key, value, Tag());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_MODIFIER_ADD_HEADER_HPP_20100824
