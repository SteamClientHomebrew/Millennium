
#ifndef BOOST_NETWORK_TRAITS_ISTREAM_HPP_20100924
#define BOOST_NETWORK_TRAITS_ISTREAM_HPP_20100924

// Copyright 2010 (C) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <boost/network/traits/char.hpp>
#include <istream>

namespace boost {
namespace network {

template <class Tag>
struct unsupported_tag;

template <class Tag, class Enable = void>
struct istream {
  typedef unsupported_tag<Tag> type;
};

template <class Tag>
struct istream<Tag, typename enable_if<is_default_string<Tag> >::type> {
  typedef std::istream type;
};

template <class Tag>
struct istream<Tag, typename enable_if<is_default_wstring<Tag> >::type> {
  typedef std::wistream type;
};

}  // namespace network
 /* network */

}  // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_TRAITS_ISTREAM_HPP_20100924 */
