//            Copyright (c) Glyn Matthews 2011, 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_DIRECTIVES_QUERY_INC__
#define BOOST_NETWORK_URI_DIRECTIVES_QUERY_INC__

#include <boost/network/uri/encode.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/empty.hpp>
#include <string>

namespace boost {
namespace network {
namespace uri {
struct query_directive {

  explicit query_directive(std::string  query) : query_(std::move(query)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    uri.append("?");
    uri.append(query_);
  }

  std::string query_;
};

inline query_directive query(const std::string &query) {
  return query_directive(query);
}

struct query_key_query_directive {

  query_key_query_directive(std::string  key, std::string  query)
      : key_(std::move(key)), query_(std::move(query)) {}

  template <class Uri>
  void operator()(Uri &uri) const {
    std::string encoded_key, encoded_query;
    if (boost::empty(uri.query())) {
      uri.append("?");
    } else {
      uri.append("&");
    }
    uri.append(key_);
    uri.append("=");
    uri.append(query_);
  }

  std::string key_;
  std::string query_;
};

inline query_key_query_directive query(const std::string &key,
                                       const std::string &query) {
  return query_key_query_directive(key, query);
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_DIRECTIVES_QUERY_INC__
