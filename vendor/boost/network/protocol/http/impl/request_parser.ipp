//
// request_parser.ipp
// ~~~~~~~~~~~~~~~~~~
//
// Implementation file for the header-only version of the request_parser.
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2009 Dean Michael Berris (mikhailberis@gmail.com)
// Copyright (c) 2009 Tarroo, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_NETWORK_HTTP_REQUEST_PARSER_IPP
#define BOOST_NETWORK_HTTP_REQUEST_PARSER_IPP

#include <boost/network/protocol/http/request.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
boost::tribool basic_request_parser<Tag>::consume(basic_request<Tag>& req,
                                                  char input) {
  switch (state_) {
    case method_start:
      if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
        return false;
      } else {
        state_ = method;
        req.method.push_back(input);
        return boost::indeterminate;
      }
    case method:
      if (input == ' ') {
        state_ = uri;
        return boost::indeterminate;
      } else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
        return false;
      } else {
        req.method.push_back(input);
        return boost::indeterminate;
      }
    case uri_start:
      if (is_ctl(input)) {
        return false;
      } else {
        state_ = uri;
        req.destination.push_back(input);
        return boost::indeterminate;
      }
    case uri:
      if (input == ' ') {
        state_ = http_version_h;
        return boost::indeterminate;
      } else if (is_ctl(input)) {
        return false;
      } else {
        req.destination.push_back(input);
        return boost::indeterminate;
      }
    case http_version_h:
      if (input == 'H') {
        state_ = http_version_t_1;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_t_1:
      if (input == 'T') {
        state_ = http_version_t_2;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_t_2:
      if (input == 'T') {
        state_ = http_version_p;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_p:
      if (input == 'P') {
        state_ = http_version_slash;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_slash:
      if (input == '/') {
        req.http_version_major = 0;
        req.http_version_minor = 0;
        state_ = http_version_major_start;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_major_start:
      if (is_digit(input)) {
        req.http_version_major = req.http_version_major * 10 + input - '0';
        state_ = http_version_major;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_major:
      if (input == '.') {
        state_ = http_version_minor_start;
        return boost::indeterminate;
      } else if (is_digit(input)) {
        req.http_version_major = req.http_version_major * 10 + input - '0';
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_minor_start:
      if (is_digit(input)) {
        req.http_version_minor = req.http_version_minor * 10 + input - '0';
        state_ = http_version_minor;
        return boost::indeterminate;
      } else {
        return false;
      }
    case http_version_minor:
      if (input == '\r') {
        state_ = expecting_newline_1;
        return boost::indeterminate;
      } else if (is_digit(input)) {
        req.http_version_minor = req.http_version_minor * 10 + input - '0';
        return boost::indeterminate;
      } else {
        return false;
      }
    case expecting_newline_1:
      if (input == '\n') {
        state_ = header_line_start;
        return boost::indeterminate;
      } else {
        return false;
      }
    case header_line_start:
      if (input == '\r') {
        state_ = expecting_newline_3;
        return boost::indeterminate;
      } else if (!req.headers.empty() && (input == ' ' || input == '\t')) {
        state_ = header_lws;
        return boost::indeterminate;
      } else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
        return false;
      } else {
        req.headers.push_back(typename request_header<Tag>::type());
        req.headers.back().name.push_back(input);
        state_ = header_name;
        return boost::indeterminate;
      }
    case header_lws:
      if (input == '\r') {
        state_ = expecting_newline_2;
        return boost::indeterminate;
      } else if (input == ' ' || input == '\t') {
        return boost::indeterminate;
      } else if (is_ctl(input)) {
        return false;
      } else {
        state_ = header_value;
        req.headers.back().value.push_back(input);
        return boost::indeterminate;
      }
    case header_name:
      if (input == ':') {
        state_ = space_before_header_value;
        return boost::indeterminate;
      } else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
        return false;
      } else {
        req.headers.back().name.push_back(input);
        return boost::indeterminate;
      }
    case space_before_header_value:
      if (input == ' ') {
        state_ = header_value;
        return boost::indeterminate;
      } else {
        return false;
      }
    case header_value:
      if (input == '\r') {
        state_ = expecting_newline_2;
        return boost::indeterminate;
      } else if (is_ctl(input)) {
        return false;
      } else {
        req.headers.back().value.push_back(input);
        return boost::indeterminate;
      }
    case expecting_newline_2:
      if (input == '\n') {
        state_ = header_line_start;
        return boost::indeterminate;
      } else {
        return false;
      }
    case expecting_newline_3:
      return (input == '\n');
    default:
      return false;
  }
}

template <class Tag>
bool basic_request_parser<Tag>::is_char(int c) {
  return c >= 0 && c <= 127;
}

template <class Tag>
bool basic_request_parser<Tag>::is_ctl(int c) {
  return (c >= 0 && c <= 31) || (c == 127);
}

template <class Tag>
bool basic_request_parser<Tag>::is_tspecial(int c) {
  switch (c) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
      return true;
    default:
      return false;
  }
}

template <class Tag>
bool basic_request_parser<Tag>::is_digit(int c) {
  return c >= '0' && c <= '9';
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_HTTP_REQUEST_PARSER_IPP
