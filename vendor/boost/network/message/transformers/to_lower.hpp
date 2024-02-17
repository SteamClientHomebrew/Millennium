
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_TRANSFORMERS_TO_LOWER_HPP__
#define BOOST_NETWORK_MESSAGE_TRANSFORMERS_TO_LOWER_HPP__

#include <boost/algorithm/string.hpp>
#include <boost/network/message/transformers/selectors.hpp>
#include <boost/network/message_fwd.hpp>

/** to_lower.hpp
 *
 * Implements the to_lower transformer. This applies
 * the to_lower string algorithm to a string, which
 * is selected by the appropriate selector.
 *
 * This defines a type, to be applied using template
 * metaprogramming on the selected string target.
 */
namespace boost {
namespace network {

namespace impl {

template <class Selector>
struct to_lower_transformer {};

template <>
struct to_lower_transformer<selectors::source_selector> {
  template <class Tag>
  void operator()(basic_message<Tag> &message_) const {
    ::boost::to_lower(message_.source());
  }

 protected:
  ~to_lower_transformer() = default;
};

template <>
struct to_lower_transformer<selectors::destination_selector> {
  template <class Tag>
  void operator()(basic_message<Tag> &message_) const {
    ::boost::to_lower(message_.destination());
  }

 protected:
  ~to_lower_transformer() = default;
};

}  // namespace impl

namespace detail {
struct to_lower_placeholder_helper;
}  // namespace detail

detail::to_lower_placeholder_helper to_lower_(
    detail::to_lower_placeholder_helper /*unused*/);

namespace detail {

struct to_lower_placeholder_helper {
  template <class Selector>
  struct type : public impl::to_lower_transformer<Selector> {};

 private:
  to_lower_placeholder_helper() = default;
  to_lower_placeholder_helper(to_lower_placeholder_helper const & /*unused*/) {}
  friend to_lower_placeholder_helper boost::network::to_lower_(
      to_lower_placeholder_helper /*unused*/);
};
}  // namespace detail

typedef detail::to_lower_placeholder_helper (*to_lower_placeholder)(
    detail::to_lower_placeholder_helper);

inline detail::to_lower_placeholder_helper to_lower_(
    detail::to_lower_placeholder_helper /*unused*/) {
  return detail::to_lower_placeholder_helper();
}

}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_TRANSFORMERS_TO_LOWER_HPP__
