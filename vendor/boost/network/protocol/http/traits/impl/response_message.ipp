
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP

#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

template <>
struct response_message<tags::http_default_8bit_tcp_resolve> {
  static char const* ok() {
    static char const* const RC_OK = "OK";
    return RC_OK;
  };

  static char const* created() {
    static char const* const RC_CREATED = "Created";
    return RC_CREATED;
  };

  static char const* no_content() {
    static char const* const RC_NO_CONTENT = "NO Content";
    return RC_NO_CONTENT;
  };

  static char const* unauthorized() {
    static char const* const RC_UNAUTHORIZED = "Unauthorized";
    return RC_UNAUTHORIZED;
  };

  static char const* forbidden() {
    static char const* const RC_FORBIDDEN = "Fobidden";
    return RC_FORBIDDEN;
  };

  static char const* not_found() {
    static char const* const RC_NOT_FOUND = "Not Found";
    return RC_NOT_FOUND;
  };

  static char const* method_not_allowed() {
    static char const* const RC_METHOD_NOT_ALLOWED = "Method Not Allowed";
    return RC_METHOD_NOT_ALLOWED;
  };

  static char const* not_modified() {
    static char const* const RC_NOT_MODIFIED = "Not Modified";
    return RC_NOT_MODIFIED;
  };

  static char const* bad_request() {
    static char const* const RC_BAD_REQUEST = "Bad Request";
    return RC_BAD_REQUEST;
  };

  static char const* server_error() {
    static char const* const RC_SERVER_ERROR = "Server Error";
    return RC_SERVER_ERROR;
  };

  static char const* not_implemented() {
    static char const* const RC_NOT_IMPLEMENTED = "Not Implemented";
    return RC_NOT_IMPLEMENTED;
  };
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP
