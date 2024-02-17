#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_PORT_HPP_20100618
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_PORT_HPP_20100618

// Copyright 2010, 2014 Dean Michael Berris <dberris@google.com>
// Copyright 2010 (c) Sinefunc, Inc.
// Copyright 2014 Google, Inc.
// Copyright 2014 Jussi Lyytinen
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <boost/network/uri/accessors.hpp>
#include <boost/optional.hpp>
#include <boost/version.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

namespace impl {

template <class Tag>
struct port_wrapper {
  basic_request<Tag> const& message_;

  explicit port_wrapper(basic_request<Tag> const& message) : message_(message) {}

  typedef typename basic_request<Tag>::port_type port_type;

  operator port_type() const { return message_.port(); }

#if (_MSC_VER >= 1600 && BOOST_VERSION > 105500)
  // Because of a breaking change in Boost 1.56 to boost::optional, implicit
  // conversions no longer work correctly with MSVC. The conversion therefore
  // has to be done explicitly with as_optional().
  boost::optional<std::uint16_t> as_optional() const {
    return uri::port_us(message_.uri());
  }
#else
  operator boost::optional<std::uint16_t>() const {
    return uri::port_us(message_.uri());
  }
#endif

};

}  // namespace impl

template <class Tag>
inline impl::port_wrapper<Tag> port(basic_request<Tag> const& request) {
  return impl::port_wrapper<Tag>(request);
}

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_PORT_HPP_20100618
