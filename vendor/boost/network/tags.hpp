// Copyright Dean Michael Berris 2008, 2009.
// Copyright Google, Inc. 2015
// Glyn Matthews 2009
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_TAG_INCLUDED_20100808
#define BOOST_NETWORK_TAG_INCLUDED_20100808

#include <boost/mpl/vector.hpp>
#include <boost/mpl/inherit.hpp>
#include <boost/mpl/inherit_linearly.hpp>
#include <boost/mpl/placeholders.hpp>

namespace boost {
namespace network {
namespace tags {

struct pod {
  typedef mpl::true_::type is_pod;
};
struct normal {
  typedef mpl::true_::type is_normal;
};
struct async {
  typedef mpl::true_::type is_async;
};
struct tcp {
  typedef mpl::true_::type is_tcp;
};
struct udp {
  typedef mpl::true_::type is_udp;
};
struct default_string {
  typedef mpl::true_::type is_default_string;
};
struct default_wstring {
  typedef mpl::true_::type is_default_wstring;
};

template <class Tag>
struct components;

// Tag Definition Macro Helper
#ifndef BOOST_NETWORK_DEFINE_TAG
#define BOOST_NETWORK_DEFINE_TAG(name)                                        \
  struct name                                                                 \
      : mpl::inherit_linearly<name##_tags,                                    \
                              mpl::inherit<mpl::placeholders::_1,             \
                                           mpl::placeholders::_2> >::type {}; \
  template <>                                                                 \
  struct components<name> {                                                   \
    typedef name##_tags type;                                                 \
  };
#endif  // BOOST_NETWORK_DEFINE_TAG

typedef default_string default_;

}  // namespace tags

}  // namespace network

}  // namespace boost

#endif  // __BOOST_NETWORK_TAGS_INC__
