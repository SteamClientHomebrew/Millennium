#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_STATUS_HPP_20100603
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_STATUS_HPP_20100603

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <boost/mpl/if.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/tags.hpp>
#include <boost/thread/future.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_response;

struct status_directive {

  boost::variant<std::uint16_t, boost::shared_future<std::uint16_t> >
      status_;

  explicit status_directive(std::uint16_t status) : status_(status) {}

  explicit status_directive(boost::shared_future<std::uint16_t> const &status)
      : status_(status) {}

  status_directive(status_directive const &other) : status_(other.status_) {}

  template <class Tag>
  struct value : mpl::if_<is_async<Tag>, boost::shared_future<std::uint16_t>,
                          std::uint16_t> {};

  template <class Tag>
  struct status_visitor : boost::static_visitor<> {
    basic_response<Tag> const &response;
    explicit status_visitor(basic_response<Tag> const &response) : response(response) {}

    void operator()(typename value<Tag>::type const &status_) const {
      response.status(status_);
    }

    template <class T>
    void operator()(T const & /*unused*/) const {
      // FIXME fail here!
    }
  };

  template <class Tag>
  basic_response<Tag> const &operator()(basic_response<Tag> const &response)
      const {
    apply_visitor(status_visitor<Tag>(response), status_);
    return response;
  }
};

template <class T>
inline status_directive const status(T const &status_) {
  return status_directive(status_);
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_DIRECTIVES_STATUS_HPP_20100603
