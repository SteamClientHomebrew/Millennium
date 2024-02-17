//
// header.hpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2009,2010 Dean Michael Berris (mikhailberis@gmail.com)
// Copyright (c) 2009 Tarroo, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_HPP_20101122
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_HPP_20101122

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <boost/network/support/is_default_wstring.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct unsupported_tag;

struct request_header_narrow {
  typedef std::string string_type;
  std::string name, value;
};

struct request_header_wide {
  typedef std::wstring string_type;
  std::wstring name, value;
};

template <class Tag>
struct request_header
    : mpl::if_<is_default_string<Tag>, request_header_narrow,
               typename mpl::if_<is_default_wstring<Tag>, request_header_wide,
                                 unsupported_tag<Tag> >::type> {};

inline void swap(request_header_narrow& l, request_header_narrow& r) {
  swap(l.name, r.name);
  swap(l.value, r.value);
}

inline void swap(request_header_wide& l, request_header_wide& r) {
  swap(l.name, r.name);
  swap(l.value, r.value);
}

struct response_header_narrow {
  typedef std::string string_type;
  std::string name, value;
};

struct response_header_wide {
  typedef std::wstring string_type;
  std::wstring name, value;
};

template <class Tag>
struct response_header
    : mpl::if_<is_default_string<Tag>, response_header_narrow,
               typename mpl::if_<is_default_wstring<Tag>, response_header_wide,
                                 unsupported_tag<Tag> >::type> {};

inline void swap(response_header_narrow& l, response_header_narrow& r) {
  std::swap(l.name, r.name);
  std::swap(l.value, r.value);
}

inline void swap(response_header_wide& l, response_header_wide& r) {
  std::swap(l.name, r.name);
  std::swap(l.value, r.value);
}

}  // namespace http

}  // namespace network

}  // namespace boost

BOOST_FUSION_ADAPT_STRUCT(boost::network::http::request_header_narrow,
                          (std::string, name)(std::string, value))

BOOST_FUSION_ADAPT_STRUCT(boost::network::http::request_header_wide,
                          (std::wstring, name)(std::wstring, value))

BOOST_FUSION_ADAPT_STRUCT(boost::network::http::response_header_narrow,
                          (std::string, name)(std::string, value))

BOOST_FUSION_ADAPT_STRUCT(boost::network::http::response_header_wide,
                          (std::wstring, name)(std::wstring, value))

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HEADER_HPP_20101122
