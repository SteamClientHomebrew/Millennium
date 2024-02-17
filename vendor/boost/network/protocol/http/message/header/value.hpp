#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_VALUE_HPP_20101028
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_VALUE_HPP_20101028

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <utility>

namespace boost {
namespace network {
namespace http {

struct request_header_narrow;
struct request_header_wide;
struct response_header_narrow;
struct response_header_wide;

template <class T1, class T2>
T1& value(std::pair<T1, T2> const& p) {
  return p.second;
}

inline request_header_narrow::string_type const& value(
    request_header_narrow const& h) {
  return h.value;
}

inline request_header_wide::string_type const& value(
    request_header_wide const& h) {
  return h.value;
}

inline response_header_narrow::string_type const& value(
    response_header_narrow const& h) {
  return h.value;
}

inline response_header_wide::string_type const& value(
    response_header_wide const& h) {
  return h.value;
}

} // namespace http
 /* http */

} // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_VALUE_HPP_20101028 */
