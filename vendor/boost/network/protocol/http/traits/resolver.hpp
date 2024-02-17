#ifndef BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_RESOLVER_20091214
#define BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_RESOLVER_20091214

//          Copyright Dean Michael Berris 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/not.hpp>
#include <boost/network/support/is_tcp.hpp>
#include <boost/network/support/is_udp.hpp>
#include <boost/network/protocol/http/support/is_http.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct unsupported_tag;

template <class Tag>
struct resolver
    : mpl::if_<mpl::and_<is_tcp<Tag>, is_http<Tag> >,
               boost::asio::ip::tcp::resolver,
               typename mpl::if_<mpl::and_<is_udp<Tag>, is_http<Tag> >,
                                 boost::asio::ip::udp::resolver,
                                 unsupported_tag<Tag> >::type> {
  static_assert(mpl::not_<mpl::and_<is_udp<Tag>, is_tcp<Tag> > >::value,
                "Transport protocol must be TCP or UDP");
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_RESOLVER_20091214
