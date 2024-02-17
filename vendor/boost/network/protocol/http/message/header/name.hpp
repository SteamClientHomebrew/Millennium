#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_NAME_HPP_20101028
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_NAME_HPP_20101028

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/message/header.hpp>
#include <utility>

namespace boost {
namespace network {
namespace http {

template <class T1, class T2>
T1 &name(std::pair<T1, T2> const &p) {
  return p.first;
}

inline std::string const &name(request_header_narrow const &h) {
  return h.name;
}

inline std::wstring const &name(request_header_wide const &h) { return h.name; }

inline std::string const &name(response_header_narrow const &h) {
  return h.name;
}

inline std::wstring const &name(response_header_wide const &h) {
  return h.name;
}

}  // namespace http
 /* http */

} // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_NAME_HPP_20101028 */
