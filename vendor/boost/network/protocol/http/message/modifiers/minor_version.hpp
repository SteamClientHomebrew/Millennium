#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_MINOR_VERSION_HPP_20101120
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_MINOR_VERSION_HPP_20101120

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <boost/network/protocol/http/support/is_server.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

template <class Tag>
inline typename enable_if<is_server<Tag>, void>::type minor_version(
    basic_request<Tag>& request, std::uint8_t minor_version_) {
  request.http_version_minor = minor_version_;
}

}  // namespace http
 /* http */

}  // namespace network
 /* network */

} // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIERS_MINOR_VERSION_HPP_20101120 \
          */
