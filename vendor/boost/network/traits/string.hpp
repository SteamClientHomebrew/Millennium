//            Copyright (c) Dean Michael Berris 2008, 2009.
//                          Glyn Matthews 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_TRAITS_STRING_INC
#define BOOST_NETWORK_TRAITS_STRING_INC

#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <string>

#ifndef BOOST_NETWORK_DEFAULT_STRING
#define BOOST_NETWORK_DEFAULT_STRING std::string
#endif

#ifndef BOOST_NETWORK_DEFAULT_WSTRING
#define BOOST_NETWORK_DEFAULT_WSTRING std::wstring
#endif

namespace boost {
namespace network {

template <class Tag>
struct unsupported_tag;

template <class Tag, class Enable = void>
struct string {
  typedef unsupported_tag<Tag> type;
};

template <class Tag>
struct string<Tag, typename enable_if<is_default_string<Tag> >::type> {
  typedef BOOST_NETWORK_DEFAULT_STRING type;
};

template <class Tag>
struct string<Tag, typename enable_if<is_default_wstring<Tag> >::type> {
  typedef BOOST_NETWORK_DEFAULT_WSTRING type;
};

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_TRAITS_STRING_INC
