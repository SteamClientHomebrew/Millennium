#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_ASYNC_MESSAGE_HPP_20100622
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_ASYNC_MESSAGE_HPP_20100622

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <boost/optional.hpp>

// FIXME move this out to a trait
#include <set>
#include <boost/network/detail/wrapper_base.hpp>
#include <boost/thread/future.hpp>

namespace boost {
namespace network {
namespace http {

namespace impl {

template <class Tag>
struct ready_wrapper;

} // namespace impl
 /* impl */

template <class Tag>
struct async_message {

  typedef typename string<Tag>::type string_type;
  typedef typename headers_container<Tag>::type headers_container_type;
  typedef typename headers_container_type::value_type header_type;

  async_message()
      : status_message_(),
        version_(),
        source_(),
        destination_(),
        status_(),
        headers_(),
        retrieved_headers_(),
        added_headers(),
        removed_headers(),
        body_() {}

  async_message(async_message const& other)
      : status_message_(other.status_message_),
        version_(other.version_),
        source_(other.source_),
        destination_(other.destination_),
        status_(other.status_),
        headers_(other.headers_),
        retrieved_headers_(other.retrieved_headers_),
        added_headers(other.added_headers),
        removed_headers(other.removed_headers),
        body_(other.body_) {}

  string_type const status_message() const { return status_message_.get(); }

  void status_message(boost::shared_future<string_type> const& future) const {
    status_message_ = future;
  }

  string_type const version() const { return version_.get(); }

  void version(boost::shared_future<string_type> const& future) const {
    version_ = future;
  }

  std::uint16_t status() const { return status_.get(); }

  void status(boost::shared_future<uint16_t> const& future) const {
    status_ = future;
  }

  string_type const source() const { return source_.get(); }

  void source(boost::shared_future<string_type> const& future) const {
    source_ = future;
  }

  string_type const destination() const { return destination_.get(); }

  void destination(boost::shared_future<string_type> const& future) const {
    destination_ = future;
  }

  headers_container_type const& headers() const {
    if (retrieved_headers_) return *retrieved_headers_;
    headers_container_type raw_headers = headers_.get();
    raw_headers.insert(added_headers.begin(), added_headers.end());
    for (string_type const & key : removed_headers) {
      raw_headers.erase(key);
    }
    retrieved_headers_ = boost::make_optional(raw_headers);
    return *retrieved_headers_;
  }

  void headers(boost::shared_future<headers_container_type> const& future)
      const {
    headers_ = future;
  }

  void add_header(typename headers_container_type::value_type const& pair_)
      const {
    added_headers.insert(added_headers.end(), pair_);
  }

  void remove_header(typename headers_container_type::key_type const& key_)
      const {
    removed_headers.insert(key_);
  }

  string_type const body() const { return body_.get(); }

  void body(boost::shared_future<string_type> const& future) const {
    body_ = future;
  }

  void swap(async_message& other) {
    std::swap(status_message_, other.status_message_);
    std::swap(status_, other.status_);
    std::swap(version_, other.version_);
    std::swap(source_, other.source_);
    std::swap(destination_, other.destination_);
    std::swap(headers_, other.headers_);
    std::swap(retrieved_headers_, other.retrieved_headers_);
    std::swap(added_headers, other.added_headers);
    std::swap(removed_headers, other.removed_headers);
    std::swap(body_, other.body_);
  }

  async_message& operator=(async_message other) {
    other.swap(*this);
    return *this;
  }

 private:
  mutable boost::shared_future<string_type> status_message_, version_, source_,
      destination_;
  mutable boost::shared_future<std::uint16_t> status_;
  mutable boost::shared_future<headers_container_type> headers_;
  mutable boost::optional<headers_container_type> retrieved_headers_;
  mutable headers_container_type added_headers;
  mutable std::set<string_type> removed_headers;
  mutable boost::shared_future<string_type> body_;

  friend struct boost::network::http::impl::ready_wrapper<Tag>;
};

template <class Tag>
inline void swap(async_message<Tag>& lhs, async_message<Tag>& rhs) {
  lhs.swap(rhs);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_ASYNC_MESSAGE_HPP_20100622
