// Copyright 2009, 2010, 2011, 2012 Dean Michael Berris, Jeroen Habraken, Glyn
// Matthews.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_URI_INC__
#define BOOST_NETWORK_URI_INC__

#pragma once

#include <iterator>
#include <functional>
#include <boost/network/uri/config.hpp>
#include <boost/network/uri/detail/uri_parts.hpp>
#include <boost/network/uri/schemes.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/utility/swap.hpp>
#include <boost/range/as_literal.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/optional.hpp>
#include <boost/functional/hash_fwd.hpp>

namespace boost {
namespace network {
namespace uri {
namespace detail {
bool parse(std::string::const_iterator first, std::string::const_iterator last,
           uri_parts<std::string::const_iterator> &parts);
}  // namespace detail

class BOOST_URI_DECL uri {

  friend class builder;

 public:
  typedef std::string string_type;
  typedef string_type::value_type value_type;
  typedef string_type::const_iterator const_iterator;
  typedef boost::iterator_range<const_iterator> const_range_type;

  uri() : is_valid_(false) {}

  // uri(const value_type *uri)
  //    : uri_(uri), is_valid_(false) {
  //    parse();
  //}

  uri(string_type str) : uri_(std::move(str)), is_valid_(false) { parse(); }

  template <class FwdIter>
  uri(const FwdIter &first, const FwdIter &last)
      : uri_(first, last), is_valid_(false) {
    parse();
  }

  uri(const uri &other) : uri_(other.uri_) { parse(); }

  uri &operator=(const uri &other) {
    uri(other).swap(*this);
    return *this;
  }

  uri &operator=(const string_type &uri_string) {
    uri(uri_string).swap(*this);
    return *this;
  }

  ~uri() = default;

  void swap(uri &other) {
    boost::swap(uri_, other.uri_);
    other.parse();
    boost::swap(is_valid_, other.is_valid_);
  }

  const_iterator begin() const { return uri_.begin(); }

  const_iterator end() const { return uri_.end(); }

  const_range_type scheme_range() const { return uri_parts_.scheme; }

  const_range_type user_info_range() const {
    return uri_parts_.hier_part.user_info ? uri_parts_.hier_part.user_info.get()
                                          : const_range_type();
  }

  const_range_type host_range() const {
    return uri_parts_.hier_part.host ? uri_parts_.hier_part.host.get()
                                     : const_range_type();
  }

  const_range_type port_range() const {
    return uri_parts_.hier_part.port ? uri_parts_.hier_part.port.get()
                                     : const_range_type();
  }

  const_range_type path_range() const {
    return uri_parts_.hier_part.path ? uri_parts_.hier_part.path.get()
                                     : const_range_type();
  }

  const_range_type query_range() const {
    return uri_parts_.query ? uri_parts_.query.get() : const_range_type();
  }

  const_range_type fragment_range() const {
    return uri_parts_.fragment ? uri_parts_.fragment.get() : const_range_type();
  }

// hackfix by Simon Haegler, Esri R&D Zurich
// this workaround is needed to avoid running into the "incompatible string
// iterator" assertion triggered by the default-constructed string iterators
// employed by cpp-netlib (see uri.ipp qi::rule declarations)
#if defined(_MSC_VER) && defined(_DEBUG)
#	define CATCH_EMPTY_ITERATOR_RANGE if (range.begin()._Getcont() == 0 || range.end()._Getcont() == 0) { return string_type(); }
#else
#	define CATCH_EMPTY_ITERATOR_RANGE
#endif

  string_type scheme() const {
    const_range_type range = scheme_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type user_info() const {
    const_range_type range = user_info_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type host() const {
    const_range_type range = host_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type port() const {
    const_range_type range = port_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type path() const {
    const_range_type range = path_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type query() const {
    const_range_type range = query_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

  string_type fragment() const {
    const_range_type range = fragment_range();
    CATCH_EMPTY_ITERATOR_RANGE
    return range ? string_type(std::begin(range), std::end(range))
                 : string_type();
  }

#ifdef CATCH_EMPTY_ITERATOR_RANGE
#undef CATCH_EMPTY_ITERATOR_RANGE
#endif

  string_type string() const { return uri_; }

  bool is_valid() const { return is_valid_; }

  void append(const string_type &data) {
    uri_.append(data);
    parse();
  }

  template <class FwdIter>
  void append(const FwdIter &first, const FwdIter &last) {
    uri_.append(first, last);
    parse();
  }

 private:
  void parse();

  string_type uri_;
  detail::uri_parts<const_iterator> uri_parts_;
  bool is_valid_;
};

inline void uri::parse() {
  const_iterator first(std::begin(uri_)), last(std::end(uri_));
  is_valid_ = detail::parse(first, last, uri_parts_);
  if (is_valid_) {
    if (!uri_parts_.scheme) {
      uri_parts_.scheme =
          const_range_type(std::begin(uri_), std::begin(uri_));
    }
    uri_parts_.update();
  }
}

inline uri::string_type scheme(const uri &uri_) { return uri_.scheme(); }

inline uri::string_type user_info(const uri &uri_) { return uri_.user_info(); }

inline uri::string_type host(const uri &uri_) { return uri_.host(); }

inline uri::string_type port(const uri &uri_) { return uri_.port(); }

inline boost::optional<unsigned short> port_us(const uri &uri_) {
  uri::string_type port = uri_.port();
  return (port.empty()) ? boost::optional<unsigned short>()
                        : boost::optional<unsigned short>(std::stoi(port));
}

inline uri::string_type path(const uri &uri_) { return uri_.path(); }

inline uri::string_type query(const uri &uri_) { return uri_.query(); }

inline uri::string_type fragment(const uri &uri_) { return uri_.fragment(); }

inline uri::string_type hierarchical_part(const uri &uri_) {
  uri::string_type::const_iterator first, last;
  uri::const_range_type user_info = uri_.user_info_range();
  uri::const_range_type host = uri_.host_range();
  uri::const_range_type port = uri_.port_range();
  uri::const_range_type path = uri_.path_range();
  if (user_info) {
    first = std::begin(user_info);
  } else {
    first = std::begin(host);
  }
  if (path) {
    last = std::end(path);
  } else if (port) {
    last = std::end(port);
  } else {
    last = std::end(host);
  }
  return uri::string_type(first, last);
}

inline uri::string_type authority(const uri &uri_) {
  uri::string_type::const_iterator first, last;
  uri::const_range_type user_info = uri_.user_info_range();
  uri::const_range_type host = uri_.host_range();
  uri::const_range_type port = uri_.port_range();
  if (user_info) {
    first = std::begin(user_info);
  } else {
    first = std::begin(host);
  }

  if (port) {
    last = std::end(port);
  } else {
    last = std::end(host);
  }
  return uri::string_type(first, last);
}

inline bool valid(const uri &uri_) { return uri_.is_valid(); }

inline bool is_absolute(const uri &uri_) {
  return uri_.is_valid() && !boost::empty(uri_.scheme_range());
}

inline bool is_relative(const uri &uri_) {
  return uri_.is_valid() && boost::empty(uri_.scheme_range());
}

inline bool is_hierarchical(const uri &uri_) {
  return is_absolute(uri_) && hierarchical_schemes::exists(scheme(uri_));
}

inline bool is_opaque(const uri &uri_) {
  return is_absolute(uri_) && opaque_schemes::exists(scheme(uri_));
}

inline bool is_valid(const uri &uri_) { return valid(uri_); }

inline void swap(uri &lhs, uri &rhs) { lhs.swap(rhs); }

inline std::size_t hash_value(const uri &uri_) {
  std::size_t seed = 0;
  for (uri::const_iterator it = begin(uri_); it != end(uri_); ++it) {
    hash_combine(seed, *it);
  }
  return seed;
}
} // namespace uri
} // namespace network
} // namespace boost

namespace std {
template <>
struct hash<boost::network::uri::uri> {

  std::size_t operator()(const boost::network::uri::uri &uri_) const {
    std::size_t seed = 0;
    std::for_each(std::begin(uri_), std::end(uri_),
                  [&seed](boost::network::uri::uri::value_type v) {
                    std::hash<boost::network::uri::uri::value_type> hasher;
                    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                  });
    return seed;
  }
};
}  // namespace std

namespace boost {
namespace network {
namespace uri {
inline bool operator==(const uri &lhs, const uri &rhs) {
  return boost::equal(lhs, rhs);
}

inline bool operator==(const uri &lhs, const uri::string_type &rhs) {
  return boost::equal(lhs, rhs);
}

inline bool operator==(const uri::string_type &lhs, const uri &rhs) {
  return boost::equal(lhs, rhs);
}

inline bool operator==(const uri &lhs, const uri::value_type *rhs) {
  auto rlen = std::strlen(rhs);
  size_t llen = std::labs(std::distance(lhs.begin(), lhs.end()));
  if (rlen != llen) return false;
  return boost::equal(lhs, boost::make_iterator_range(rhs, rhs + rlen));
}

inline bool operator==(const uri::value_type *lhs, const uri &rhs) {
  return boost::equal(boost::as_literal(lhs), rhs);
}

inline bool operator!=(const uri &lhs, const uri &rhs) { return !(lhs == rhs); }

inline bool operator<(const uri &lhs, const uri &rhs) {
  return lhs.string() < rhs.string();
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#include <boost/network/uri/accessors.hpp>
#include <boost/network/uri/directives.hpp>
#include <boost/network/uri/builder.hpp>

namespace boost {
namespace network {
namespace uri {
inline uri from_parts(const uri &base_uri, const uri::string_type &path_,
                      const uri::string_type &query_,
                      const uri::string_type &fragment_) {
  uri uri_(base_uri);
  builder(uri_).path(path_).query(query_).fragment(fragment_);
  return uri_;
}

inline uri from_parts(const uri &base_uri, const uri::string_type &path_,
                      const uri::string_type &query_) {
  uri uri_(base_uri);
  builder(uri_).path(path_).query(query_);
  return uri_;
}

inline uri from_parts(const uri &base_uri, const uri::string_type &path_) {
  uri uri_(base_uri);
  builder(uri_).path(path_);
  return uri_;
}

inline uri from_parts(const uri::string_type &base_uri,
                      const uri::string_type &path,
                      const uri::string_type &query,
                      const uri::string_type &fragment) {
  return from_parts(uri(base_uri), path, query, fragment);
}

inline uri from_parts(const uri::string_type &base_uri,
                      const uri::string_type &path,
                      const uri::string_type &query) {
  return from_parts(uri(base_uri), path, query);
}

inline uri from_parts(const uri::string_type &base_uri,
                      const uri::string_type &path) {
  return from_parts(uri(base_uri), path);
}
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_URI_INC__
