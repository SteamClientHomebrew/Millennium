#ifndef BOOST_NETWORK_PROTOCOL_HTTP_RESOLVER_POLICY_20091214
#define BOOST_NETWORK_PROTOCOL_HTTP_RESOLVER_POLICY_20091214

//          Copyright Dean Michael Berris 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/policies/sync_resolver.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/protocol/http/policies/async_resolver.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/if.hpp>
#include <boost/network/protocol/http/support/is_http.hpp>
#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct unsupported_tag;

template <class Tag>
struct resolver_policy
    : mpl::if_<mpl::and_<is_async<Tag>, is_http<Tag> >,
               policies::async_resolver<Tag>,
               typename mpl::if_<is_http<Tag>, policies::sync_resolver<Tag>,
                                 unsupported_tag<Tag> >::type> {};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_RESOLVER_POLICY_20091214
