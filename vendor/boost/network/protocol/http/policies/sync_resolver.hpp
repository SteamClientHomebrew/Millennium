#ifndef BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SYNC_RESOLVER_20091214
#define BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SYNC_RESOLVER_20091214

//          Copyright Dean Michael Berris 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <unordered_map>
#include <utility>
#include <tuple>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/network/protocol/http/traits/resolver.hpp>
#include <boost/network/traits/string.hpp>

namespace boost {
namespace network {
namespace http {
namespace policies {

template <class Tag>
struct sync_resolver {

  typedef typename resolver<Tag>::type resolver_type;
  typedef typename resolver_type::iterator resolver_iterator;
  typedef typename resolver_type::query resolver_query;
  typedef std::pair<resolver_iterator, resolver_iterator>
      resolver_iterator_pair;

 protected:
  typedef typename string<Tag>::type string_type;
  typedef std::unordered_map<string_type, resolver_iterator_pair>
      resolved_cache;
  resolved_cache endpoint_cache_;
  bool cache_resolved_;

  explicit sync_resolver(bool cache_resolved) : cache_resolved_(cache_resolved) {}

  resolver_iterator_pair resolve(resolver_type& resolver_,
                                 string_type  /*unused*/const& hostname,
                                 string_type const& port) {
    if (cache_resolved_) {
      typename resolved_cache::iterator cached_iterator =
          endpoint_cache_.find(hostname);
      if (cached_iterator == endpoint_cache_.end()) {
        bool inserted = false;
        std::tie(cached_iterator, inserted) =
            endpoint_cache_.insert(std::make_pair(
                boost::to_lower_copy(hostname),
                std::make_pair(
                    resolver_.resolve(resolver_query(
                        hostname, port, resolver_query::numeric_service)),
                    resolver_iterator())));
      };
      return cached_iterator->second;
    };

    return std::make_pair(resolver_.resolve(resolver_query(
                              hostname, port, resolver_query::numeric_service)),
                          resolver_iterator());
  };
};

}  // namespace policies

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_SYNC_RESOLVER_20091214
