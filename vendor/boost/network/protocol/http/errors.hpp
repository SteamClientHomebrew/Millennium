
//          Copyright Dean Michael Berris 2007, 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_PROTOCOL_HTTP_ERRORS_20080516_HPP__
#define __NETWORK_PROTOCOL_HTTP_ERRORS_20080516_HPP__

#include <boost/network/protocol/http/message.hpp>
#include <exception>

namespace boost {
namespace network {
namespace http {
namespace errors {

template <class Tag = tags::http_default_8bit_tcp_resolve>
struct connection_timeout_exception : std::runtime_error {};

typedef connection_timeout_exception<> connection_timeout;

}  // namespace errors

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // __NETWORK_PROTOCOL_HTTP_20080516_HPP__
