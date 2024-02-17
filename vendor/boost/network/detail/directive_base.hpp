
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_DETAIL_DIRECTIVE_BASE_HPP__
#define __NETWORK_DETAIL_DIRECTIVE_BASE_HPP__

#include <boost/network/message_fwd.hpp>

/** Defines the base type from which all directives inherit
 * to allow friend access to message and other types' internals.
 */
namespace boost {
namespace network {
namespace detail {

template <class Tag>
struct directive_base {
  typedef Tag tag;
  explicit directive_base(basic_message<tag> & message)
      : message_(message) {}

 protected:
  ~directive_base() = default;  // can only be extended

  basic_message<tag> & message_;
};

}  // namespace detail

}  // namespace network

}  // namespace boost

#endif  // __NETWORK_DETAIL_DIRECTIVE_BASE_HPP__
