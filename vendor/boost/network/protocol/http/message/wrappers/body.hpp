#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_BODY_HPP_20100622
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_BODY_HPP_20100622

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

template <class Tag>
struct basic_request;

namespace impl {

template <class Message>
struct body_wrapper {
  typedef typename string<typename Message::tag>::type string_type;
  Message const& message_;
  explicit body_wrapper(Message const& message) : message_(message) {}
  body_wrapper(body_wrapper const& other) : message_(other.message_) {}

  operator string_type() const { return message_.body(); }

  size_t size() const { return message_.body().size(); }

  boost::iterator_range<typename string_type::const_iterator> range() const {
    return boost::make_iterator_range(message_.body());
  }
};

template <class Message>
inline std::ostream& operator<<(std::ostream& os,
                                body_wrapper<Message> const& body) {
  os << static_cast<typename body_wrapper<Message>::string_type>(body);
  return os;
}

}  // namespace impl

template <class Tag>
inline typename impl::body_wrapper<basic_response<Tag> > body(
    basic_response<Tag> const& message) {
  return impl::body_wrapper<basic_response<Tag> >(message);
}

template <class Tag>
inline typename impl::body_wrapper<basic_request<Tag> > body(
    basic_request<Tag> const& message) {
  return impl::body_wrapper<basic_request<Tag> >(message);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_BODY_HPP_20100622
