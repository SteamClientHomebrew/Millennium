//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_DIRECTIVES_SCHEME_INC__
#define __BOOST_NETWORK_URI_DIRECTIVES_SCHEME_INC__

#include <boost/network/uri/schemes.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <string>

namespace boost {
namespace network {
namespace uri {
struct scheme_directive {

  explicit scheme_directive(std::string scheme) : scheme(std::move(scheme)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append(scheme);
    if (opaque_schemes::exists(scheme)) {
      uri.append(":");
    } else {
      uri.append("://");
    }
  }

  std::string scheme;
};

inline scheme_directive scheme(const std::string &scheme) {
  return scheme_directive(scheme);
}

namespace schemes {
inline uri &http(uri &uri_) { return uri_ << scheme("http"); }

inline uri &https(uri &uri_) { return uri_ << scheme("https"); }

inline uri &file(uri &uri_) { return uri_ << scheme("file"); }
}  // namespace schemes
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_URI_DIRECTIVES_SCHEME_INC__
