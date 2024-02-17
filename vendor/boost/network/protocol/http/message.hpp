// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// Copyright 2015 Google, Inc.
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// Some changes Copyright (c) Dean Michael Berris 2008

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HPP

#include <string>
#include <cstdint>
#include <boost/network/protocol/http/traits.hpp>
#include <boost/network/protocol/http/message/header/name.hpp>
#include <boost/network/protocol/http/message/header/value.hpp>
#include <boost/network/message.hpp>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {
namespace http {

/// base class for HTTP messages (requests and responses)
template <typename Tag>
struct message_impl : public basic_message<Tag> {

  typedef typename string<Tag>::type string_type;

  /// escapes URL-encoded strings (a%20value+with%20spaces)
  static string_type const url_decode(string_type const &str);

  /// encodes strings so that they are safe for URLs (with%20spaces)
  static string_type const url_encode(string_type const &str);

  /// builds an HTTP query string from a collection of query parameters
  static string_type const make_query_string(
      typename query_container<Tag>::type const &query_params);

  /**
   * creates a "Set-Cookie" header
   *
   * @param name the name of the cookie
   * @param value the value of the cookie
   * @param path the path of the cookie
   * @param has_max_age true if the max_age value should be set
   * @param max_age the life of the cookie, in seconds (0 = discard)
   *
   * @return the new "Set-Cookie" header
   */
  static string_type const make_set_cookie_header(
      string_type const &name, string_type const &value,
      string_type const &path, bool const has_max_age = false,
      unsigned long const max_age = 0);

  /** decodes base64-encoded strings
   *
   * @param input base64 encoded string
   * @param output decoded string ( may include non-text chars)
   * @return true if successful, false if input string contains non-base64
   *symbols
   */
  static bool base64_decode(string_type const &input, string_type &output);

  /** encodes strings using base64
   *
   * @param input arbitrary string ( may include non-text chars)
   * @param output base64 encoded string
   * @return true if successful
   */
  static bool base64_encode(string_type const &input, string_type &output);

 protected:
  mutable string_type version_;
  mutable std::uint16_t status_;
  mutable string_type status_message_;

 private:
  typedef basic_message<Tag> base_type;

 public:
  message_impl() : base_type(), version_(), status_(0u), status_message_() {}

  message_impl(message_impl const &other)
      : base_type(other),
        version_(other.version_),
        status_(other.status_),
        status_message_(other.status_message_) {}

  void version(string_type  /*unused*/const &version) const { version_ = version; }

  string_type const version() const { return version_; }

  void status(std::uint16_t status) const { status_ = status; }

  std::uint16_t status() const { return status_; }

  void status_message(string_type const &status_message) const {
    status_message_ = status_message;
  }

  string_type const status_message() const { return status_message_; }

  message_impl &operator=(message_impl rhs) {
    rhs.swap(*this);
    return *this;
  }

  void swap(message_impl &other) {
    base_type &base_ref(other), &this_ref(*this);
    std::swap(this_ref, base_ref);
    std::swap(status_, other.status_);
    std::swap(status_message_, other.status_message_);
    std::swap(version_, other.version_);
  }
};

template <class Tag>
inline void swap(message_impl<Tag> &lhs, message_impl<Tag> &rhs) {
  lhs.swap(rhs);
}

typedef message_impl<tags::http_default_8bit_tcp_resolve> message;

}  // namespace http

}  // namespace network

}  // namespace boost

// import implementation file
#include <boost/network/protocol/http/impl/message.ipp>

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HPP
