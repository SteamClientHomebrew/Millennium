#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_METHOD_HPP_20101118
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_METHOD_HPP_20101118

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/traits/string.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

template <class Tag>
inline typename enable_if<is_server<Tag>, void>::type method(
    basic_request<Tag>& request, typename string<Tag>::type const& method_) {
  request.method = method_;
}

} // namespace http
 /* http */

}  // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_METHOD_HPP_20101118 */
