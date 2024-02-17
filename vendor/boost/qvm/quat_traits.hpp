#ifndef BOOST_QVM_QUAT_TRAITS
#define BOOST_QVM_QUAT_TRAITS

// Copyright 2008-2022 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/is_scalar.hpp>

namespace boost { namespace qvm {

template <class Q>
struct
quat_traits
    {
    typedef void scalar_type;
    };

template <class T>
struct
is_quat
    {
    static bool const value = is_scalar<typename quat_traits<T>::scalar_type>::value;
    };

} }

#endif
