
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP

#include <cstdint>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

/* This glob doesn't have a specialization on the
 * tags::http_default_8bit_tcp_resolve
 * yet because it doesn't need to define different behaviour/types
 * on different message tags -- for example, it doesn't need to
 * determine the type or change the values of the data no matter
 * what the tag type is provided.
 */
template <class Tag>
struct response_code {
  static std::uint16_t const RC_OK = 200u;
  static std::uint16_t const RC_CREATED = 201u;
  static std::uint16_t const RC_NO_CONTENT = 204u;
  static std::uint16_t const RC_UNAUTHORIZED = 401u;
  static std::uint16_t const RC_FORBIDDEN = 403u;
  static std::uint16_t const RC_NOT_FOUND = 404u;
  static std::uint16_t const RC_METHOD_NOT_ALLOWED = 405u;
  static std::uint16_t const RC_NOT_MODIFIED = 304u;
  static std::uint16_t const RC_BAD_REQUEST = 400u;
  static std::uint16_t const RC_SERVER_ERROR = 500u;
  static std::uint16_t const RC_NOT_IMPLEMENTED = 501u;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP
