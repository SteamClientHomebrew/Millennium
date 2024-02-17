#ifndef BOOST_NETWORK_DEBUG_HPP_20110410
#define BOOST_NETWORK_DEBUG_HPP_20110410

// (c) Copyright 2011 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/** BOOST_NETWORK_MESSAGE is a debugging macro used by cpp-netlib to
    print out network-related errors through standard error. This is
    only useful when BOOST_NETWORK_DEBUG is turned on. Otherwise
    the macro amounts to a no-op.
*/
#ifdef BOOST_NETWORK_DEBUG
#include <iostream>
#ifndef BOOST_NETWORK_MESSAGE
#define BOOST_NETWORK_MESSAGE(msg)                                      \
  std::cerr << "[DEBUG " << __FILE__ << ':' << __LINE__ << "]: " << msg \
            << std::endl;
#endif
#else
#ifndef BOOST_NETWORK_MESSAGE
#define BOOST_NETWORK_MESSAGE(msg)
#endif
#endif

#endif /* end of include guard: BOOST_NETWORK_DEBUG_HPP_20110410 */
