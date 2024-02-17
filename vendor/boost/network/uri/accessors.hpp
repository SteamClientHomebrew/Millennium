//            Copyright (c) Glyn Matthews 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_URI_ACCESSORS_INC__
#define BOOST_NETWORK_URI_URI_ACCESSORS_INC__

#include <iterator>
#include <boost/network/uri/encode.hpp>
#include <boost/network/uri/uri.hpp>
#include <boost/network/uri/decode.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/std_pair.hpp>

namespace boost {
namespace network {
namespace uri {
namespace details {
template <typename Map>
struct key_value_sequence : spirit::qi::grammar<uri::const_iterator, Map()> {
  typedef typename Map::key_type key_type;
  typedef typename Map::mapped_type mapped_type;
  typedef std::pair<key_type, mapped_type> pair_type;

  key_value_sequence() : key_value_sequence::base_type(query) {
    query = pair >> *((spirit::qi::lit(';') | '&') >> pair);
    pair = key >> -('=' >> value);
    key =
        spirit::qi::char_("a-zA-Z_") >> *spirit::qi::char_("-+.~a-zA-Z_0-9/%");
    value = *spirit::qi::char_("-+.~a-zA-Z_0-9/%");
  };

  spirit::qi::rule<uri::const_iterator, Map()> query;
  spirit::qi::rule<uri::const_iterator, pair_type()> pair;
  spirit::qi::rule<uri::const_iterator, key_type()> key;
  spirit::qi::rule<uri::const_iterator, mapped_type()> value;
};
}  // namespace details

template <class Map>
inline Map &query_map(const uri &uri_, Map &map) {
  const uri::string_type range = uri_.query();
  details::key_value_sequence<Map> parser;
  spirit::qi::parse(std::begin(range), std::end(range), parser, map);
  return map;
}

inline uri::string_type username(const uri &uri_) {
  const uri::string_type user_info = uri_.user_info();
  uri::const_iterator it(std::begin(user_info)), end(std::end(user_info));
  for (; it != end; ++it) {
    if (*it == ':') {
      break;
    }
  }
  return uri::string_type(std::begin(user_info), it);
}

inline uri::string_type password(const uri &uri_) {
  const uri::string_type user_info = uri_.user_info();
  uri::const_iterator it(std::begin(user_info)), end(std::end(user_info));
  for (; it != end; ++it) {
    if (*it == ':') {
      ++it;
      break;
    }
  }
  return uri::string_type(it, std::end(user_info));
}

inline uri::string_type decoded_path(const uri &uri_) {
  const uri::string_type path = uri_.path();
  uri::string_type decoded_path;
  decode(path, std::back_inserter(decoded_path));
  return decoded_path;
}

inline uri::string_type decoded_query(const uri &uri_) {
  const uri::string_type query = uri_.query();
  uri::string_type decoded_query;
  decode(query, std::back_inserter(decoded_query));
  return decoded_query;
}

inline uri::string_type decoded_fragment(const uri &uri_) {
  const uri::string_type fragment = uri_.fragment();
  uri::string_type decoded_fragment;
  decode(fragment, std::back_inserter(decoded_fragment));
  return decoded_fragment;
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_URI_ACCESSORS_INC__
