//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_DIRECTIVES_PORT_INC__
#define BOOST_NETWORK_URI_DIRECTIVES_PORT_INC__

#include <cstdint>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

namespace boost {
namespace network {
namespace uri {
struct port_directive {

  explicit port_directive(std::string port) : port_(std::move(port)) {}

  explicit port_directive(std::uint16_t port)
    : port_(std::to_string(port)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append(":");
    uri.append(port_);
  }

  std::string port_;
};

inline port_directive port(const std::string &port) {
  return port_directive(port);
}

inline port_directive port(std::uint16_t port) {
  return port_directive(port);
}

}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_DIRECTIVES_PORT_INC__
