#ifndef BOOST_NETWORK_PROTOCOL_HTTP_SUPPORT_POD_OR_NORMAL_HPP_20101128
#define BOOST_NETWORK_PROTOCOL_HTTP_SUPPORT_POD_OR_NORMAL_HPP_20101128

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/support/is_pod.hpp>
#include <boost/network/tags.hpp>

namespace boost {
namespace network {

template <class Tag, class Enable = void>
struct pod_or_normal {
  typedef tags::normal type;
};

template <class Tag>
struct pod_or_normal<
    Tag, typename enable_if<typename Tag::is_pod>::type> : tags::pod {};

}  // namespace network
}  // namespace boost

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_SUPPORT_POD_OR_NORMAL_HPP_20101128 */
