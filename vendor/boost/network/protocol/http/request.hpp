
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_PROTOCOL_HTTP_REQUEST_20070908_1_HPP__
#define __NETWORK_PROTOCOL_HTTP_REQUEST_20070908_1_HPP__

#include <boost/network/protocol/http/impl/request.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, class Directive>
basic_request<Tag>& operator<<(basic_request<Tag>& message,
                               Directive const& directive) {
  directive(message);
  return message;
}

}  // namespace http
}  // namespace network
}  // namespace boost


#endif  // __NETWORK_PROTOCOL_HTTP_REQUEST_20070908-1_HPP__
