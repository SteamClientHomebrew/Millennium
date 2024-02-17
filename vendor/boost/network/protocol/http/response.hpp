// Copyright Dean Michael Berris 2007.
// Copyright Michael Dickey 2008.
// Copyright Google, Inc. 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_RESPONSE_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_RESPONSE_HPP

#include <boost/network/protocol/http/message.hpp>
#include <boost/network/protocol/http/message/async_message.hpp>
#include <boost/network/protocol/http/message/directives/status.hpp>
#include <boost/network/protocol/http/message/directives/status_message.hpp>
#include <boost/network/protocol/http/message/directives/uri.hpp>
#include <boost/network/protocol/http/message/directives/version.hpp>
#include <boost/network/protocol/http/message/message_base.hpp>
#include <boost/network/protocol/http/message/modifiers/body.hpp>
#include <boost/network/protocol/http/message/modifiers/destination.hpp>
#include <boost/network/protocol/http/message/modifiers/headers.hpp>
#include <boost/network/protocol/http/message/modifiers/source.hpp>
#include <boost/network/protocol/http/message/modifiers/status.hpp>
#include <boost/network/protocol/http/message/modifiers/status_message.hpp>
#include <boost/network/protocol/http/message/modifiers/uri.hpp>
#include <boost/network/protocol/http/message/modifiers/version.hpp>
#include <boost/network/protocol/http/message/traits/version.hpp>
#include <boost/network/protocol/http/message/wrappers/destination.hpp>
#include <boost/network/protocol/http/message/wrappers/ready.hpp>
#include <boost/network/protocol/http/message/wrappers/source.hpp>
#include <boost/network/protocol/http/message/wrappers/status.hpp>
#include <boost/network/protocol/http/message/wrappers/status_message.hpp>
#include <boost/network/protocol/http/message/wrappers/version.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response : public message_base<Tag>::type {

  typedef typename string<Tag>::type string_type;

 private:
  typedef typename message_base<Tag>::type base_type;

 public:
  typedef Tag tag;

  basic_response() : base_type() {}

  basic_response(basic_response const& other) : base_type(other) {}

  basic_response& operator=(basic_response rhs) {
    rhs.swap(*this);
    return *this;
  };

  void swap(basic_response& other) {
    base_type& base_ref(other), &this_ref(*this);
    std::swap(this_ref, base_ref);
  };
};

template <class Tag>
inline void swap(basic_response<Tag>& lhs, basic_response<Tag>& rhs) {
  lhs.swap(rhs);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#include <boost/network/protocol/http/impl/response.ipp>

namespace boost {
namespace network {
namespace http {

template <class Tag, class Directive>
basic_response<Tag>& operator<<(basic_response<Tag>& message,
                                Directive const& directive) {
  directive(message);
  return message;
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_RESPONSE_HPP
