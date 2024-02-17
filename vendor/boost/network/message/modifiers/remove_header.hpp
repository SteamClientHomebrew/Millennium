
#ifndef BOOST_NETWORK_MESSAGE_MODIFIER_REMOVE_HEADER_HPP_20100824
#define BOOST_NETWORK_MESSAGE_MODIFIER_REMOVE_HEADER_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/not.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_pod.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace boost {
namespace network {

namespace impl {

template <class Message, class KeyType, class Tag>
inline typename enable_if<
    mpl::and_<mpl::not_<is_pod<Tag> >, mpl::not_<is_async<Tag> > >, void>::type
remove_header(Message& message, KeyType const& key, Tag) {
  message.headers().erase(key);
}

template <class Message, class KeyType, class Tag>
inline typename enable_if<mpl::and_<mpl::not_<is_pod<Tag> >, is_async<Tag> >,
                          void>::type
remove_header(Message& message, KeyType const& key, Tag) {
  message.remove_header(key);
}

template <class KeyType>
struct iequals_pred {
  KeyType const& key;
  iequals_pred(KeyType const& key) : key(key) {}
  template <class Header>
  bool operator()(Header& other) const {
    return boost::algorithm::iequals(key, name(other));
  }
};

template <class Message, class KeyType, class Tag>
inline typename enable_if<is_pod<Tag>, void>::type remove_header(
    Message& message, KeyType const& key, Tag) {
  message.headers.erase(
      boost::remove_if(message.headers, iequals_pred<KeyType>(key)),
      message.headers.end());
}

}  // namespace impl

template <class Tag, template <class> class Message, class KeyType>
inline void remove_header(Message<Tag>& message, KeyType const& key) {
  impl::remove_header(message, key, Tag());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_MODIFIER_REMOVE_HEADER_HPP_20100824
