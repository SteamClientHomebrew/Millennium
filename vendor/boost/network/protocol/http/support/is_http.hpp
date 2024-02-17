#ifndef BOOST_NETWORK_SUPPORT_IS_HTTP_HPP_20100622
#define BOOST_NETWORK_SUPPORT_IS_HTTP_HPP_20100622

// Copyright 2010 (c) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/tags.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, class Enable = void>
struct is_http : mpl::false_ {};

template <class Tag>
struct is_http<Tag,
               typename enable_if<typename Tag::is_http>::type> : mpl::true_ {};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_SUPPORT_IS_HTTP_HPP_20100622
