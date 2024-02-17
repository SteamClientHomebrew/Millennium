#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_MESSAGE_HPP_20100603
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_MESSAGE_HPP_20100603

// Copyright 2010 (c) Dean Michael Berris <dberris@google.com>
// Copyright 2010 (c) Sinefunc, Inc.
// Copyright 2014 (c) Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/traits/string.hpp>
#include <sstream>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

namespace impl {

template <class Tag>
struct status_message_wrapper {

  typedef typename string<Tag>::type string_type;

  basic_response<Tag> const& response_;

  explicit status_message_wrapper(basic_response<Tag> const& response)
      : response_(response) {}

  status_message_wrapper(status_message_wrapper const& other)
      : response_(other.response_) {}

  operator string_type() const { return response_.status_message(); }
};

template <class Tag>
inline std::ostream& operator<<(std::ostream&  os,
                                const status_message_wrapper<Tag>&  wrapper) {
  return os << static_cast<typename string<Tag>::type>(wrapper);
}

}  // namespace impl

template <class Tag>
inline impl::status_message_wrapper<Tag> status_message(
    basic_response<Tag> const& response) {
  return impl::status_message_wrapper<Tag>(response);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPER_STATUS_MESSAGE_HPP_20100603
