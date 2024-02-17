//              Copyright 2012 Glyn Matthews.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_SCHEMES_INC__
#define __BOOST_NETWORK_URI_SCHEMES_INC__

#include <string>

namespace boost {
namespace network {
namespace uri {
class hierarchical_schemes {

 public:
  static bool exists(const std::string &scheme);
};

class opaque_schemes {

 public:
  static bool exists(const std::string &scheme);
};
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_URI_SCHEMES_INC__
