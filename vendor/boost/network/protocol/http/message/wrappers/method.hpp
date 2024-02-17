#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_METHOD_HPP_20101118
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_METHOD_HPP_20101118

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/support/is_server.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

template <class Tag>
struct method_wrapper {
  explicit method_wrapper(basic_request<Tag> const& message)
      : message_(message) {}

  basic_request<Tag> const& message_;

  typedef typename basic_request<Tag>::string_type string_type;

  operator string_type() { return message_.method; }
};

template <class Tag>
inline typename enable_if<is_server<Tag>, typename string<Tag>::type>::type
method(basic_request<Tag> const& message) {
  return method_wrapper<Tag>(message);
}

} // namespace http
 /* http */

} // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_METHOD_HPP_20101118 */
