#ifndef BOOST_NETWORK_MESSAGE_MODIFIER_CLEAR_HEADERS_HPP_20100824
#define BOOST_NETWORK_MESSAGE_MODIFIER_CLEAR_HEADERS_HPP_20100824

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/and.hpp>
#include <boost/mpl/not.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_pod.hpp>
#include <boost/thread/future.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {

namespace impl {
template <class Message, class Tag>
inline typename enable_if<
    mpl::and_<mpl::not_<is_pod<Tag> >, mpl::not_<is_async<Tag> > >, void>::type
clear_headers(Message const &message, Tag const &) {
  (typename Message::headers_container_type()).swap(message.headers());
}

template <class Message, class Tag>
inline typename enable_if<is_pod<Tag>, void>::type clear_headers(
    Message const &message, Tag const &) {
  (typename Message::headers_container_type()).swap(message.headers);
}

template <class Message, class Tag>
inline typename enable_if<mpl::and_<mpl::not_<is_pod<Tag> >, is_async<Tag> >,
                          void>::type
clear_headers(Message const &message, Tag const &) {
  boost::promise<typename Message::headers_container_type> header_promise;
  boost::shared_future<typename Message::headers_container_type> headers_future(
      header_promise.get_future());
  message.headers(headers_future);
  header_promise.set_value(typename Message::headers_container_type());
}

}  // namespace impl

template <class Tag, template <class> class Message>
inline void clear_headers(Message<Tag> const &message) {
  impl::clear_headers(message, Tag());
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_MODIFIER_CLEAR_HEADERS_HPP_20100824
