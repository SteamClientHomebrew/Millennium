//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2009 Dean Michael Berris (mikhailberis@gmail.com)
// Copyright (c) 2009 Tarro, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_NETWORK_HTTP_REQUEST_HPP
#define BOOST_NETWORK_HTTP_REQUEST_HPP

#include <string>
#include <vector>
#include <algorithm>
#include "header.hpp"

namespace boost {
namespace network {
namespace http {

/// A request received from a client.
struct request {
  std::string method;
  std::string uri;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
  std::string body;
};

inline void swap(request& l, request& r) {
  using std::swap;
  swap(l.method, r.method);
  swap(l.uri, r.uri);
  swap(l.http_version_major, r.http_version_major);
  swap(l.http_version_minor, r.http_version_minor);
  swap(l.headers, r.headers);
  swap(l.body, r.body);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_HTTP_REQUEST_HPP
