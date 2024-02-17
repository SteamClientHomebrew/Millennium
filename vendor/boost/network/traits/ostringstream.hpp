//            Copyright (c) Glyn Matthews 2009.
//         Copyright (c) Dean Michael Berris 2009, 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_TRAITS_OSTRINGSTREAM_INC
#define BOOST_NETWORK_TRAITS_OSTRINGSTREAM_INC

#include <boost/mpl/if.hpp>
#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <sstream>

namespace boost {
namespace network {

template <class Tag>
struct unsupported_tag;

template <class Tag, class Enable = void>
struct ostringstream {
  typedef unsupported_tag<Tag> type;
};

template <class Tag>
struct ostringstream<Tag, typename enable_if<is_default_string<Tag> >::type> {
  typedef std::ostringstream type;
};

template <class Tag>
struct ostringstream<Tag, typename enable_if<is_default_wstring<Tag> >::type> {
  typedef std::wostringstream type;
};

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_TRAITS_OSTRINGSTREAM_INC
