// Copyright Dean Michael Berris 2010
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_SUPPORT_STRING_CHECK_20100808
#define BOOST_NETWORK_SUPPORT_STRING_CHECK_20100808

#include <boost/network/tags.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {

template <class Tag, class Enable = void>
struct is_default_string : mpl::false_ {};

template <class Tag>
struct is_default_string<
    Tag,
    typename enable_if<typename Tag::is_default_string>::type> : mpl::true_ {};

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_SUPPORT_STRING_CHECK_20100808
