//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_DIRECTIVES_HOST_INC__
#define BOOST_NETWORK_URI_DIRECTIVES_HOST_INC__

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <string>

namespace boost {
namespace network {
namespace uri {
struct host_directive {

  explicit host_directive(std::string  host) : host_(std::move(host)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append(host_);
  }

  std::string host_;
};

inline host_directive host(const std::string &host) {
  return host_directive(host);
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_DIRECTIVES_HOST_INC__
