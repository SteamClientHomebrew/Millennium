#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_MINOR_VERSION_HPP_20101120
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_MINOR_VERSION_HPP_20101120

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
struct minor_version_wrapper {
  basic_request<Tag> const& request;
  explicit minor_version_wrapper(basic_request<Tag> const& request)
      : request(request) {}
  operator std::uint8_t() { return request.http_version_minor; }
};

template <class Tag>
inline typename enable_if<is_server<Tag>, minor_version_wrapper<Tag> >::type
minor_version(basic_request<Tag> const& request) {
  return minor_version_wrapper<Tag>(request);
}

} // namespace http
 /* http */

}  // namespace network
 /* network */

} // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_MINOR_VERSION_HPP_20101120 \
          */
