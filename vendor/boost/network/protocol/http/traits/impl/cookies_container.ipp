
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_COOKIES_CONTAINER_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_COOKIES_CONTAINER_IPP

#include <boost/network/tags.hpp>

#include <map>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct cookies_container {
  typedef std::multimap<typename string<Tag>::type, typename string<Tag>::type>
      type;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_COOKIES_CONTAINER_IPP
