#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_BODY_HPP_20100624
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_BODY_HPP_20100624

// Copyright 2010 (C) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/concept/requires.hpp>
#include <boost/network/message/directives.hpp>
#include <boost/network/protocol/http/support/client_or_server.hpp>
#include <boost/network/protocol/http/tags.hpp>
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
void body(basic_response<Tag> &response, T const &value, mpl::false_ const & /*unused*/) {
  response << ::boost::network::body(value);
}

template <class Tag, class T>
void body(basic_response<Tag> &response, T const &future, mpl::true_ const & /*unused*/) {
  response.body(future);
}
}  // namespace impl

template <class Tag, class T>
inline void body(basic_response<Tag> &response, T const &value) {
  impl::body(response, value, is_async<Tag>());
}

template <class Tag, class T>
inline void body_impl(basic_request<Tag> &request, T const &value,
                      tags::server /*unused*/) {
  request.body = value;
}

template <class Tag, class T>
inline void body_impl(basic_request<Tag> &request, T const &value,
                      tags::client /*unused*/) {
  request << ::boost::network::body(value);
}

template <class Tag, class T>
inline void body(basic_request<Tag> &request, T const &value) {
  body_impl(request, value, typename client_or_server<Tag>::type());
}

}  // namespace http

namespace impl {

template <class Message, class ValueType, class Async>
inline void body(Message const &message, ValueType const &body_,
                 http::tags::http_server /*unused*/, Async /*unused*/) {
  message.body = body_;
}

} // namespace impl
 /* impl */

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_MODIFIER_BODY_HPP_20100624
