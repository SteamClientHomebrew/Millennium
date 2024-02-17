#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_VERSION_HPP_20100903
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_VERSION_HPP_20100903

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/or.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_sync.hpp>
#include <boost/network/tags.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/thread/future.hpp>

namespace boost {
namespace network {
namespace http {

namespace traits {

template <class Tag>
struct unsupported_tag;

template <class Message, class Enable = void>
struct version {
  typedef unsupported_tag<typename Message::tag> type;
};

template <class Message>
struct version<Message,
               typename enable_if<is_async<typename Message::tag> >::type> {
  typedef boost::shared_future<typename string<typename Message::tag>::type>
      type;
};

template <class Message>
struct version<
    Message, typename enable_if<
                 mpl::or_<is_sync<typename Message::tag>,
                          is_default_string<typename Message::tag>,
                          is_default_wstring<typename Message::tag> > >::type> {
  typedef typename string<typename Message::tag>::type type;
};

}  // namespace traits
 /* traits */

}  // namespace http
 /* http */
}  // namespace network
 /* network */
}  // namespace boost
 /* boost */

#endif
