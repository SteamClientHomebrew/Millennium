
// Copyright 2013 Google, Inc.
// Copyright 2008 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_CONTAINER_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_CONTAINER_IPP

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/traits/headers_container.hpp>
#include <boost/utility/enable_if.hpp>
#include <map>
#include <string>

namespace boost {
namespace network {
namespace impl {

// Moving implementation from original
// message_traits implementation by
// Atomic Labs, Inc.
// --
// returns true if str1 < str2 (ignoring case)
struct is_less_ignore_case_impl {
  inline bool operator()(
      string<http::tags::http_default_8bit_tcp_resolve>::type  /*unused*/const& str1,
      string<http::tags::http_default_8bit_tcp_resolve>::type const& str2)
      const {
    return to_lower_copy(str1) < to_lower_copy(str2);
  };
};

template <class Tag>
struct headers_container_impl<
    Tag, typename enable_if<typename Tag::is_client>::type> {

  typedef is_less_ignore_case_impl is_less_ignore_case;

  typedef std::multimap<string<http::tags::http_default_8bit_tcp_resolve>::type,
                        string<http::tags::http_default_8bit_tcp_resolve>::type,
                        is_less_ignore_case> type;
};

}  // namespace impl
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_HEADERS_CONTAINER_IPP
