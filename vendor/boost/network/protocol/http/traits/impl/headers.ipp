
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_HPP

#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

template <>
struct headers_<tags::http_default_8bit_tcp_resolve> {
  static char const* host() {
    static char const* const HOST = "Host";
    return HOST;
  };

  static char const* cookie() {
    static char const* const COOKIE = "Cookie";
    return COOKIE;
  };

  static char const* set_cookie() {
    static char const* const SET_COOKIE = "Set-Cookie";
    return SET_COOKIE;
  };

  static char const* connection() {
    static char const* const CONNECTION = "Connection";
    return CONNECTION;
  };

  static char const* content_type() {
    static char const* const CONTENT_TYPE = "Content-Type";
    return CONTENT_TYPE;
  };

  static char const* content_length() {
    static char const* const CONTENT_LENGTH = "Content-Length";
    return CONTENT_LENGTH;
  };

  static char const* content_location() {
    static char const* const CONTENT_LOCATION = "Content-Location";
    return CONTENT_LOCATION;
  };

  static char const* last_modified() {
    static char const* const LAST_MODIFIED = "Last-Modified";
    return LAST_MODIFIED;
  };

  static char const* if_modified_since() {
    static char const* const IF_MODIFIED_SINCE = "If-Modified-Since";
    return IF_MODIFIED_SINCE;
  };

  static char const* transfer_encoding() {
    static char const* const TRANSFER_ENCODING = "Transfer-Encoding";
    return TRANSFER_ENCODING;
  };

  static char const* location() {
    static char const* const LOCATION = "Location";
    return LOCATION;
  };

  static char const* authorization() {
    static char const* const AUTHORIZATION = "Authorization";
    return AUTHORIZATION;
  };
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_HPP
