//            Copyright (c) Dean Michael Berris 2008, 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_TRAITS_CHAR_HPP
#define BOOST_NETWORK_TRAITS_CHAR_HPP

#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>

namespace boost {
namespace network {

template <class Tag>
struct unsupported_tag;

template <class Tag, class Enable = void>
struct char_ {
  typedef unsupported_tag<Tag> type;
};

template <class Tag>
struct char_<Tag, typename enable_if<is_default_string<Tag> >::type> {
  typedef char type;
};

template <class Tag>
struct char_<Tag, typename enable_if<is_default_wstring<Tag> >::type> {
  typedef wchar_t type;
};

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_TRAITS_CHAR_HPP
