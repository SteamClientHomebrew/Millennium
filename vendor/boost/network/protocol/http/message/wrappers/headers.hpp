
#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPER_HEADERS_HPP_20100811
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPER_HEADERS_HPP_20100811

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/traits/headers_container.hpp>
#include <boost/network/traits/string.hpp>

namespace boost {
namespace network {
namespace http {

template <class Message>
struct headers_range {
  typedef typename headers_container<typename Message::tag>::type
      headers_container_type;
  typedef typename boost::iterator_range<
      typename headers_container_type::const_iterator> type;
};

template <class Tag>
struct basic_request;

template <class Tag>
struct basic_response;

namespace impl {

template <class Tag>
struct request_headers_wrapper {
  typedef typename string<Tag>::type string_type;
  typedef typename headers_range<basic_request<Tag> >::type range_type;
  typedef typename headers_container<Tag>::type headers_container_type;
  typedef typename headers_container_type::const_iterator const_iterator;
  typedef typename headers_container_type::iterator iterator;

  explicit request_headers_wrapper(basic_request<Tag> const& message)
      : message_(message) {}

  range_type operator[](string_type  /*unused*/const& key) const {
    return message_.headers().equal_range(key);
  }

  typename headers_container_type::size_type count(string_type  /*unused*/const& key)
      const {
    return message_.headers().count(key);
  }

  const_iterator begin() const { return message_.headers().begin(); }

  const_iterator end() const { return message_.headers().end(); }

  operator range_type() {
    return make_iterator_range(message_.headers().begin(),
                               message_.headers().end());
  }

  operator headers_container_type() { return message_.headers(); }

 private:
  basic_request<Tag> const& message_;
};

template <class Tag>
struct response_headers_wrapper {
  typedef typename string<Tag>::type string_type;
  typedef typename headers_range<basic_response<Tag> >::type range_type;
  typedef typename headers_container<Tag>::type headers_container_type;
  typedef typename headers_container_type::const_iterator const_iterator;
  typedef typename headers_container_type::iterator iterator;

  explicit response_headers_wrapper(basic_response<Tag> const& message)
      : message_(message) {}

  range_type operator[](string_type  /*unused*/const& key) const {
    return message_.headers().equal_range(key);
  }

  typename headers_container_type::size_type count(string_type  /*unused*/const& key)
      const {
    return message_.headers().count(key);
  }

  const_iterator begin() const { return message_.headers().begin(); }

  const_iterator end() const { return message_.headers().end(); }

  operator range_type() {
    return make_iterator_range(message_.headers().begin(),
                               message_.headers().end());
  }

  operator headers_container_type() { return message_.headers(); }

 private:
  basic_response<Tag> const& message_;
};

}  // namespace impl

template <class Tag>
inline impl::request_headers_wrapper<Tag> headers(
    basic_request<Tag> const& request_) {
  return impl::request_headers_wrapper<Tag>(request_);
}

template <class Tag>
inline impl::response_headers_wrapper<Tag> headers(
    basic_response<Tag> const& response_) {
  return impl::response_headers_wrapper<Tag>(response_);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPER_HEADERS_HPP_20100811
