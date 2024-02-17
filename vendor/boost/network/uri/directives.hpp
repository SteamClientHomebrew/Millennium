//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_DIRECTIVES_INC__
#define __BOOST_NETWORK_URI_DIRECTIVES_INC__

#include <iterator>
#include <boost/network/uri/uri.hpp>

namespace boost {
namespace network {
namespace uri {
inline uri &operator<<(uri &uri_, const uri &root_uri) {
  if (empty(uri_) && valid(root_uri)) {
    uri_.append(std::begin(root_uri), std::end(root_uri));
  }
  return uri_;
}

template <class Directive>
inline uri &operator<<(uri &uri_, const Directive &directive) {
  directive(uri_);
  return uri_;
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#include <boost/network/uri/directives/scheme.hpp>
#include <boost/network/uri/directives/user_info.hpp>
#include <boost/network/uri/directives/host.hpp>
#include <boost/network/uri/directives/port.hpp>
#include <boost/network/uri/directives/authority.hpp>
#include <boost/network/uri/directives/path.hpp>
#include <boost/network/uri/directives/query.hpp>
#include <boost/network/uri/directives/fragment.hpp>

#endif  // __BOOST_NETWORK_URI_DIRECTIVES_INC__
