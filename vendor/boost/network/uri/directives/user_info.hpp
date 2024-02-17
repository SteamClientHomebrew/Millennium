//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_DIRECTIVES_USER_INFO_INC__
#define BOOST_NETWORK_URI_DIRECTIVES_USER_INFO_INC__

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <string>

namespace boost {
namespace network {
namespace uri {
struct user_info_directive {

  explicit user_info_directive(std::string user_info)
      : user_info_(std::move(user_info)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append(user_info_);
    uri.append("@");
  }

  std::string user_info_;
};

inline user_info_directive user_info(const std::string &user_info) {
  return user_info_directive(user_info);
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_DIRECTIVES_USER_INFO_INC__
