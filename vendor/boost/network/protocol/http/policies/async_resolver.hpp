#ifndef BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_ASYNC_RESOLVER_20100622
#define BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_ASYNC_RESOLVER_20100622

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/strand.hpp>
#include <boost/network/protocol/http/traits/resolver.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace boost {
namespace network {
namespace http {
namespace policies {

template <class Tag>
struct async_resolver : std::enable_shared_from_this<async_resolver<Tag> > {
  typedef typename resolver<Tag>::type resolver_type;
  typedef typename resolver_type::iterator resolver_iterator;
  typedef typename resolver_type::query resolver_query;
  typedef std::pair<resolver_iterator, resolver_iterator>
      resolver_iterator_pair;
  typedef typename string<Tag>::type string_type;
  typedef std::unordered_map<string_type, resolver_iterator_pair>
      endpoint_cache;
  typedef std::function<void(boost::system::error_code const &, resolver_iterator_pair)>
      resolve_completion_function;
  typedef std::function<void(resolver_type &, string_type, std::uint16_t,
                             resolve_completion_function)> resolve_function;

 protected:
  bool cache_resolved_;
  endpoint_cache endpoint_cache_;
  std::shared_ptr<boost::asio::io_service> service_;
  std::shared_ptr<boost::asio::io_service::strand> resolver_strand_;

  explicit async_resolver(bool cache_resolved)
      : cache_resolved_(cache_resolved), endpoint_cache_() {}

  void resolve(resolver_type &resolver_, string_type const &host,
               std::uint16_t port, resolve_completion_function once_resolved) {
    if (cache_resolved_) {
      typename endpoint_cache::iterator iter =
          endpoint_cache_.find(boost::to_lower_copy(host));
      if (iter != endpoint_cache_.end()) {
        boost::system::error_code ignored;
        once_resolved(ignored, iter->second);
        return;
      }
    }

    typename resolver_type::query q(host, std::to_string(port));
    auto self = this->shared_from_this();
    resolver_.async_resolve(
        q, resolver_strand_->wrap([=](boost::system::error_code const &ec,
                                      resolver_iterator endpoint_iterator) {
          self->handle_resolve(boost::to_lower_copy(host), once_resolved, ec,
                               endpoint_iterator);
        }));
  }

  void handle_resolve(string_type /*unused*/ const &host,
                      resolve_completion_function once_resolved,
                      boost::system::error_code const &ec,
                      resolver_iterator endpoint_iterator) {
    typename endpoint_cache::iterator iter;
    bool inserted = false;
    if (!ec && cache_resolved_) {
      std::tie(iter, inserted) = endpoint_cache_.insert(std::make_pair(
          host, std::make_pair(endpoint_iterator, resolver_iterator())));
      once_resolved(ec, iter->second);
    } else {
      once_resolved(ec, std::make_pair(endpoint_iterator, resolver_iterator()));
    }
  }
};

}  // namespace policies
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_POLICIES_ASYNC_RESOLVER_20100622
