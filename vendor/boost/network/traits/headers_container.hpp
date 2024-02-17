// Copyright (c) Glyn Matthews 2009.
// Copyright 2013 Google, Inc.
// Copyright 2013 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_TRAITS_HEADERS_CONTAINER_INC
#define BOOST_NETWORK_TRAITS_HEADERS_CONTAINER_INC

#include <boost/network/tags.hpp>
#include <boost/network/traits/string.hpp>
#include <map>

namespace boost {
namespace network {
namespace impl {

template <class Tag, class Enable = void>
struct headers_container_impl {
  typedef std::multimap<typename string<Tag>::type, typename string<Tag>::type>
      type;
};

}  // namespace impl

template <class Tag>
struct headers_container : impl::headers_container_impl<Tag> {};

}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_TRAITS_HEADERS_CONTAINER_INC__
