#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_HPP_20100603
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_HPP_20100603

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

namespace impl {

template <class Tag>
struct status_wrapper {

  basic_response<Tag> const& response_;

  explicit status_wrapper(basic_response<Tag> const& response)
      : response_(response) {}

  status_wrapper(status_wrapper const& other) : response_(other.response_) {}

  operator std::uint16_t() { return response_.status(); }
};

}  // namespace impl

template <class R>
struct Response;

template <class Tag>
inline impl::status_wrapper<Tag> status(basic_response<Tag> const& response) {
  return impl::status_wrapper<Tag>(response);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_HPP_20100603
