
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CONTENT_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CONTENT_IPP

#include <boost/network/protocol/http/tags.hpp>

namespace boost {
namespace network {
namespace http {

template <>
struct content<tags::http_default_8bit_tcp_resolve> {
  static char const* type_html() {
    static char const* const TYPE_HTML = "text/html";
    return TYPE_HTML;
  };

  static char const* type_text() {
    static char const* const TYPE_TEXT = "text/plain";
    return TYPE_TEXT;
  };

  static char const* type_xml() {
    static char const* const TYPE_XML = "text/xml";
    return TYPE_XML;
  };

  static char const* type_urlencoded() {
    static char const* const TYPE_URLENCODED =
        "application/x-www-form-urlencoded";
    return TYPE_URLENCODED;
  };
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CONTENT_IPP
