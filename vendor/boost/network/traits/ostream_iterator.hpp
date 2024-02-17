#ifndef BOOST_NETWORK_TRAITS_OSTREAM_ITERATOR_HPP_20100815
#define BOOST_NETWORK_TRAITS_OSTREAM_ITERATOR_HPP_20100815

// Copyright 2010 (C) Dean Michael Berris
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/mpl/if.hpp>
#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <iterator>

namespace boost {
namespace network {

template <class Tag>
struct unsupported_tag;

template <class Tag, class Input>
struct ostream_iterator;

template <class Tag, class Input>
struct ostream_iterator
    : mpl::if_<is_default_string<Tag>, std::ostream_iterator<Input, char>,
               typename mpl::if_<is_default_wstring<Tag>,
                                 std::ostream_iterator<Input, wchar_t>,
                                 unsupported_tag<Tag> >::type> {};

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_TRAITS_OSTREAM_ITERATOR_HPP_20100815
