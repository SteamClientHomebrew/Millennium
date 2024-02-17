#ifndef BOOST_NETWORK_MESSAGE_TRAITS_SOURCE_HPP_20100903
#define BOOST_NETWORK_MESSAGE_TRAITS_SOURCE_HPP_20100903

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/if.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_sync.hpp>
#include <boost/network/tags.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/thread/future.hpp>
#include <boost/type_traits/is_same.hpp>

namespace boost {
namespace network {
namespace traits {

template <class Tag>
struct unsupported_tag;

template <class Message>
struct source
    : mpl::if_<is_async<typename Message::tag>,
               boost::shared_future<typename string<typename Message::tag>::type>,
               typename mpl::if_<
                   mpl::or_<is_sync<typename Message::tag>,
                            is_same<typename Message::tag,
                                    ::boost::network::tags::default_string>,
                            is_same<typename Message::tag,
                                    ::boost::network::tags::default_wstring> >,
                   typename string<typename Message::tag>::type,
                   unsupported_tag<typename Message::tag> >::type> {};

}  // namespace traits
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_TRAITS_SOURCE_HPP_20100903
