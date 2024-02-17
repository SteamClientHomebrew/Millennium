
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_DETAIL_WRAPPER_BASE_HPP__
#define __NETWORK_DETAIL_WRAPPER_BASE_HPP__

namespace boost {
namespace network {

namespace detail {

template <class Tag, class Message>
struct wrapper_base {
  explicit wrapper_base(Message& message_) : _message(message_) {};

 protected:
  ~wrapper_base() = default;  // for extending only
  Message& _message;
};

template <class Tag, class Message>
struct wrapper_base_const {
  explicit wrapper_base_const(Message const& message_) : _message(message_) {}

 protected:
  ~wrapper_base_const() = default;  // for extending only
  Message const& _message;
};

}  // namespace detail

}  // namespace network

}  // namespace boost

#endif  // __NETWORK_DETAIL_WRAPPER_BASE_HPP__
