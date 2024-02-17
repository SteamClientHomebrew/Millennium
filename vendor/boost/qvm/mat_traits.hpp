#ifndef BOOST_QVM_TRAITS_HPP_INCLUDED
#define BOOST_QVM_TRAITS_HPP_INCLUDED

// Copyright 2008-2022 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/is_scalar.hpp>

namespace boost { namespace qvm {

template <class M>
struct
mat_traits
    {
    static int const rows=0;
    static int const cols=0;
    typedef void scalar_type;
    };

template <class T>
struct
is_mat
    {
    static bool const value = is_scalar<typename mat_traits<T>::scalar_type>::value && mat_traits<T>::rows>0 && mat_traits<T>::cols>0;
    };

} }

#endif
