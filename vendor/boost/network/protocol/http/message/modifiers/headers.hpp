#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_HEADERS_HPP_20100624
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_HEADERS_HPP_20100624

// Copyright 2010 (C) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/concept/requires.hpp>
#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

template <class Tag>
struct basic_request;

namespace impl {

template <class Tag, class T>
void headers(basic_response<Tag> &response, T const &value,
             mpl::false_ const & /*unused*/) {
  response << headers(value);
}

template <class Tag, class T>
void headers(basic_response<Tag> &response, T const &future,
             mpl::true_ const & /*unused*/) {
  response.headers(future);
}

template <class Tag, class T>
void headers(basic_request<Tag> &request, T const &value,
             tags::server const & /*unused*/) {
  request.headers = value;
}
} // namespace impl

template <class Tag, class T>
inline void headers(basic_response<Tag> &response, T const &value) {
  impl::headers(response, value, is_async<Tag>());
}

template <class Tag, class T>
inline void headers(basic_request<Tag> &request, T const &value) {
  impl::headers(request, value, Tag());
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_HEADERS_HPP_20100624
