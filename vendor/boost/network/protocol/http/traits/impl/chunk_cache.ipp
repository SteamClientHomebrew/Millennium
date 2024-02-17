
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CHUNK_CACHE_CONTAINER_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CHUNK_CACHE_CONTAINER_IPP

#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/traits/char.hpp>

#include <list>
#include <vector>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct chunk_cache {
  // TODO(dberris): define the allocator using an allocator_traits?
  typedef std::list<std::vector<typename char_<Tag>::type> > type;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_CHUNK_CACHE_CONTAINER_IPP
