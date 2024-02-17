#ifndef BOOST_NETWORK_SUPPORT_IS_ASYNC_HPP_20100608
#define BOOST_NETWORK_SUPPORT_IS_ASYNC_HPP_20100608

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/bool.hpp>
#include <boost/network/support/is_sync.hpp>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {

template <class Tag, class Enable = void>
struct is_async : mpl::false_ {};

template <class Tag>
struct is_async<
    Tag, typename enable_if<typename Tag::is_async>::type> : mpl::true_ {};

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_SUPPORT_IS_ASYNC_HPP_2010608
