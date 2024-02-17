// Copyright Dean Michael Berris 2007.
// Copyright Google, Inc. 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_HPP__
#define BOOST_NETWORK_MESSAGE_HPP__

#include <boost/network/detail/directive_base.hpp>
#include <boost/network/detail/wrapper_base.hpp>
#include <boost/network/traits/headers_container.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/utility/enable_if.hpp>

/** message.hpp
 *
 * This header file implements the common message type which
 * all networking implementations under the boost::network
 * namespace. The common message type allows for easy message
 * construction and manipulation suited for networked
 * application development.
 */
namespace boost {
namespace network {

/** The common message type.
 */
template <class Tag>
struct basic_message {
 public:
  typedef Tag tag;

  typedef typename headers_container<Tag>::type headers_container_type;
  typedef typename headers_container_type::value_type header_type;
  typedef typename string<Tag>::type string_type;

  basic_message() = default;
  basic_message(const basic_message&) = default;
  basic_message(basic_message&&) = default;
  basic_message& operator=(basic_message const&) = default;
  basic_message& operator=(basic_message&&) = default;
  ~basic_message() = default;

  void swap(basic_message<Tag>& other) {
    using std::swap;
    swap(other.headers_, headers_);
    swap(other.body_, body_);
    swap(other.source_, source_);
    swap(other.destination_, destination_);
  }

  headers_container_type& headers() { return headers_; }

  void headers(headers_container_type headers) const {
    headers_ = std::move(headers);
  }

  void add_header(
      typename headers_container_type::value_type const& pair_) const {
    headers_.insert(pair_);
  }

  void remove_header(
      typename headers_container_type::key_type const& key) const {
    headers_.erase(key);
  }

  headers_container_type const& headers() const { return headers_; }

  string_type& body() { return body_; }

  void body(string_type body) const { body_ = std::move(body); }

  string_type const& body() const { return body_; }

  string_type& source() { return source_; }

  void source(string_type source) const { source_ = std::move(source); }

  string_type const& source() const { return source_; }

  string_type& destination() { return destination_; }

  void destination(string_type destination) const {
    destination_ = std::move(destination);
  }

  string_type const& destination() const { return destination_; }

 private:
  friend struct detail::directive_base<Tag>;
  friend struct detail::wrapper_base<Tag, basic_message<Tag> >;

  // Define an equality operator that's only available via ADL.
  friend bool operator==(basic_message const& l, basic_message const& r) {
    return l.headers_ == r.headers_ && l.source_ == r.source_ &&
           l.destination_ == r.destination_ && l.body_ == r.body_;
  }

  // Define an inequality operator that's only available via ADL in terms of
  // equality defined above.
  friend bool operator!=(basic_message const& l, basic_message const& r) {
    return !(l == r);
  }

  mutable headers_container_type headers_;
  mutable string_type body_;
  mutable string_type source_;
  mutable string_type destination_;
};

template <class Tag>
inline void swap(basic_message<Tag>& left, basic_message<Tag>& right) {
  // swap for ADL
  left.swap(right);
}

typedef basic_message<tags::default_string> message;
typedef basic_message<tags::default_wstring> wmessage;

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_HPP__
