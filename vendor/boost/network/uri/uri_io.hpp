//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_URI_IO_INC__
#define __BOOST_NETWORK_URI_URI_IO_INC__

#include <boost/network/uri/uri.hpp>

namespace boost {
namespace network {
namespace uri {

inline std::ostream &operator<<(std::ostream &os, const uri &uri_) {
  return os << uri_.string();
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_URI_URI_IO_INC__
