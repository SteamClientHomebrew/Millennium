#ifndef BOOST_NETWORK_CONSTANTS_HPP_20100808
#define BOOST_NETWORK_CONSTANTS_HPP_20100808

// Copyright 2010 (C) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/if.hpp>
#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>

namespace boost {
namespace network {

namespace impl {
template <class Tag>
struct constants_narrow {

  static char const* crlf() {
    static char crlf_[] = {'\r', '\n', 0};
    return crlf_;
  }

  static char const* dot() {
    static char dot_[] = {'.', 0};
    return dot_;
  }

  static char dot_char() { return '.'; }

  static char const* http_slash() {
    static char http_slash_[] = {'H', 'T', 'T', 'P', '/', 0};
    return http_slash_;
  }

  static char const* space() {
    static char space_[] = {' ', 0};
    return space_;
  }

  static char space_char() { return ' '; }

  static char const* slash() {
    static char slash_[] = {'/', 0};
    return slash_;
  }

  static char slash_char() { return '/'; }

  static char const* host() {
    static char host_[] = {'H', 'o', 's', 't', 0};
    return host_;
  }

  static char const* colon() {
    static char colon_[] = {':', 0};
    return colon_;
  }

  static char colon_char() { return ':'; }

  static char const* accept() {
    static char accept_[] = {'A', 'c', 'c', 'e', 'p', 't', 0};
    return accept_;
  }

  static char const* default_accept_mime() {
    static char mime_[] = {'*', '/', '*', 0};
    return mime_;
  }

  static char const* accept_encoding() {
    static char accept_encoding_[] = {'A', 'c', 'c', 'e', 'p', 't', '-', 'E',
                                      'n', 'c', 'o', 'd', 'i', 'n', 'g', 0};
    return accept_encoding_;
  }

  static char const* default_accept_encoding() {
    static char default_accept_encoding_[] = {
        'i', 'd', 'e', 'n', 't', 'i', 't', 'y', ';', 'q', '=',
        '1', '.', '0', ',', ' ', '*', ';', 'q', '=', '0', 0};
    return default_accept_encoding_;
  }

  static char const* user_agent() {
    static char user_agent_[] = {'U', 's', 'e', 'r', '-', 'A',
                                 'g', 'e', 'n', 't', 0};
    return user_agent_;
  }

  static char const* cpp_netlib_slash() {
    static char cpp_netlib_slash_[] = {'c', 'p', 'p', '-', 'n', 'e',
                                       't', 'l', 'i', 'b', '/', 0};
    return cpp_netlib_slash_;
  }

  static char question_mark_char() { return '?'; }

  static char hash_char() { return '#'; }

  static char const* connection() {
    static char connection_[] = {'C', 'o', 'n', 'n', 'e', 'c',
                                 't', 'i', 'o', 'n', 0};
    return connection_;
  }

  static char const* close() {
    static char close_[] = {'C', 'l', 'o', 's', 'e', 0};
    return close_;
  }

  static char const* https() {
    static char https_[] = "https";
    return https_;
  }
};

template <class Tag>
struct constants_wide {

  static wchar_t const* https() {
    static wchar_t https_[] = L"https";
    return https_;
  }
};
} // namespace impl

template <class Tag>
struct unsupported_tag;

template <class Tag>
struct constants
    : mpl::if_<
          is_default_string<Tag>, impl::constants_narrow<Tag>,
          typename mpl::if_<is_default_wstring<Tag>, impl::constants_wide<Tag>,
                            unsupported_tag<Tag> >::type>::type {};

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_CONSTANTS_HPP_20100808
