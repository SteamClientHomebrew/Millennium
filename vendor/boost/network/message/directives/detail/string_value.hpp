#ifndef BOOST_NETWORK_MESSAGE_DIRECTIVES_DETAIL_STRING_VALUE_HPP_20100915
#define BOOST_NETWORK_MESSAGE_DIRECTIVES_DETAIL_STRING_VALUE_HPP_20100915

//          Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/traits/string.hpp>
#include <boost/network/support/is_async.hpp>
#include <boost/network/support/is_sync.hpp>
#include <boost/thread/future.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/or.hpp>

namespace boost {
namespace network {
namespace detail {

template <class Tag>
struct string_value
    : mpl::if_<is_async<Tag>, boost::shared_future<typename string<Tag>::type>,
               typename mpl::if_<
                   mpl::or_<is_sync<Tag>, is_same<Tag, tags::default_string>,
                            is_same<Tag, tags::default_wstring> >,
                   typename string<Tag>::type, unsupported_tag<Tag> >::type> {};

} /* detail */
} /* network */
} /* boost */

#endif /* BOOST_NETWORK_MESSAGE_DIRECTIVES_DETAIL_STRING_VALUE_HPP_20100915 */
