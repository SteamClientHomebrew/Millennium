//            Copyright (c) Glyn Matthews 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_CONFIG_INC__
#define __BOOST_NETWORK_URI_CONFIG_INC__

#include <boost/config.hpp>
#include <boost/detail/workaround.hpp>

#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_URI_DYN_LINK)
#define BOOST_URI_DECL
#else
#define BOOST_URI_DECL
#endif  // defined(BOOST_ALL_DYN_LINK) || defined(BOOST_URI_DYN_LINK)

#endif  // __BOOST_NETWORK_URI_CONFIG_INC__
