#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_MAJOR_VERSION_HPP_20101120
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_MAJOR_VERSION_HPP_20101120

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

struct major_version_directive {
  std::uint8_t major_version;
  explicit major_version_directive(std::uint8_t major_version)
      : major_version(major_version) {}
  template <class Tag>
  void operator()(basic_request<Tag>& request) const {
    request.http_version_major = major_version;
  }
};

inline major_version_directive major_version(std::uint8_t major_version_) {
  return major_version_directive(major_version_);
}

}  // namespace http
 /* http */

}  // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_MAJOR_VERSION_HPP_20101120 \
          */
