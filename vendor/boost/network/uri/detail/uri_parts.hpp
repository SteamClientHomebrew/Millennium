// Copyright 2009, 2010, 2011, 2012 Dean Michael Berris, Jeroen Habraken, Glyn
// Matthews.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URL_DETAIL_URL_PARTS_HPP_
#define BOOST_NETWORK_URL_DETAIL_URL_PARTS_HPP_

#include <iterator>
#include <boost/optional.hpp>
#include <boost/range/iterator_range.hpp>

namespace boost {
namespace network {
namespace uri {
namespace detail {
template <class FwdIter>
struct hierarchical_part {
  optional<iterator_range<FwdIter> > user_info;
  optional<iterator_range<FwdIter> > host;
  optional<iterator_range<FwdIter> > port;
  optional<iterator_range<FwdIter> > path;

  FwdIter begin() const { return std::begin(user_info); }

  FwdIter end() const { return std::end(path); }

  void update() {
    if (!user_info) {
      if (host) {
        user_info = ::boost::make_optional(
            iterator_range<FwdIter>(std::begin(host.get()),
                                    std::begin(host.get())));
      } else if (path) {
        user_info = ::boost::make_optional(
            iterator_range<FwdIter>(std::begin(path.get()),
                                    std::begin(path.get())));
      }
    }

    if (!host) {
      host = ::boost::make_optional(
          iterator_range<FwdIter>(std::begin(path.get()),
                                  std::begin(path.get())));
    }

    if (!port) {
      port = ::boost::make_optional(
          iterator_range<FwdIter>(std::end(host.get()), std::end(host.get())));
    }

    if (!path) {
      path = ::boost::make_optional(
          iterator_range<FwdIter>(std::end(port.get()), std::end(port.get())));
    }
  }
};

template <class FwdIter>
struct uri_parts {
  iterator_range<FwdIter> scheme;
  hierarchical_part<FwdIter> hier_part;
  optional<iterator_range<FwdIter> > query;
  optional<iterator_range<FwdIter> > fragment;

  FwdIter begin() const { return std::begin(scheme); }

  FwdIter end() const { return std::end(fragment); }

  void update() {

    hier_part.update();

    if (!query) {
      query = ::boost::make_optional(
          iterator_range<FwdIter>(std::end(hier_part.path.get()),
                                  std::end(hier_part.path.get())));
    }

    if (!fragment) {
      fragment = ::boost::make_optional(
          iterator_range<FwdIter>(std::end(query.get()),
                                  std::end(query.get())));
    }
  }
};
}  // namespace detail
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URL_DETAIL_URL_PARTS_HPP_
