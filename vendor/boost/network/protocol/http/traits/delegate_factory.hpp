#ifndef BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_DELEGATE_FACTORY_HPP_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_DELEGATE_FACTORY_HPP_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/is_async.hpp>

namespace boost {
namespace network {
namespace http {

namespace impl {

template <class Tag>
struct connection_delegate_factory;

} // namespace impl
 /* impl */

template <class Tag>
struct unsupported_tag;

template <class Tag, class Enable = void>
struct delegate_factory {
  typedef unsupported_tag<Tag> type;
};

template <class Tag>
struct delegate_factory<Tag, typename enable_if<is_async<Tag> >::type> {
  typedef impl::connection_delegate_factory<Tag> type;
};

} // namespace http
 /* http */
}  // namespace network
 /* network */
} // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_DELEGATE_FACTORY_HPP_20110819 */
