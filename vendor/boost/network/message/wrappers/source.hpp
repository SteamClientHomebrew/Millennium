
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_MESSAGE_WRAPPERS_SOURCE_HPP__
#define __NETWORK_MESSAGE_WRAPPERS_SOURCE_HPP__

#include <boost/network/traits/string.hpp>

namespace boost {
namespace network {

namespace impl {
template <class Tag>
struct source_wrapper : public detail::wrapper_base<Tag, basic_message<Tag> > {
  typedef Tag tag;
  typedef basic_message<tag> message_type;
  typedef typename string<tag>::type string_type;
  typedef detail::wrapper_base<Tag, basic_message<Tag> > wrapper_base;

  explicit source_wrapper(basic_message<tag>& message_)
      : wrapper_base(message_) {};

  operator string_type() const {
    return string_type(wrapper_base::_message.source());
  };
};
}  // namespace impl

template <class Tag>
inline typename string<Tag>::type source(basic_message<Tag>& message_) {
  return impl::source_wrapper<Tag>(message_);
}

}  // namespace network

}  // namespace boost

#endif  // __NETWORK_MESSAGE_WRAPPERS_SOURCE_HPP__
