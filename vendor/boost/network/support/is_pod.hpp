#ifndef BOOST_NETWORK_SUPPORT_IS_POD_HPP_20101120
#define BOOST_NETWORK_SUPPORT_IS_POD_HPP_20101120

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/tags.hpp>
#include <boost/utility/enable_if.hpp>

namespace boost {
namespace network {

template <class Tag, class Enable = void>
struct is_pod : mpl::false_ {};

template <class Tag>
struct is_pod<Tag,
              typename enable_if<typename Tag::is_pod>::type> : mpl::true_ {};

} // namespace network
 // namespace network
 /* network */

}  // namespace boost
 // namespace boost
 /* boost */

#endif /* BOOST_NETWORK_SUPPORT_IS_POD_HPP_20101120 */
