//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_DIRECTIVES_AUTHORITY_INC__
#define BOOST_NETWORK_URI_DIRECTIVES_AUTHORITY_INC__

#include <string>

namespace boost {
namespace network {
namespace uri {

struct authority_directive {

  explicit authority_directive(std::string authority)
      : authority_(std::move(authority)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append(authority_);
  }

  std::string authority_;
};

inline authority_directive authority(const std::string &authority) {
  return authority_directive(authority);
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_DIRECTIVES_AUTHORITY_INC__
