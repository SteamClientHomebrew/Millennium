
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __NETWORK_MESSAGE_TRANSFORMERS_HPP__
#define __NETWORK_MESSAGE_TRANSFORMERS_HPP__

/** transformers.hpp
 *
 * Pulls in all the transformers files.
 */
#include <boost/network/message/transformers/selectors.hpp>
#include <boost/network/message/transformers/to_upper.hpp>
#include <boost/network/message/transformers/to_lower.hpp>

#include <boost/type_traits.hpp>

namespace boost {
namespace network {
namespace impl {
template <class Algorithm, class Selector>
struct get_real_algorithm {
  typedef typename boost::function_traits<
      typename boost::remove_pointer<Algorithm>::type>::result_type::
      template type<typename boost::function_traits<
          typename boost::remove_pointer<Selector>::type>::result_type> type;
};

template <class Algorithm, class Selector>
struct transform_impl : public get_real_algorithm<Algorithm, Selector>::type {};
} // namespace impl

template <class Algorithm, class Selector>
inline impl::transform_impl<Algorithm, Selector> transform(Algorithm /*unused*/ /*unused*/,
                                                           Selector /*unused*/ /*unused*/) {
  return impl::transform_impl<Algorithm, Selector>();
}

template <class Tag, class Algorithm, class Selector>
inline basic_message<Tag>& operator<<(
    basic_message<Tag>& msg_,
    impl::transform_impl<Algorithm, Selector> const& transformer) {
  transformer(msg_);
  return msg_;
}

}  // namespace network

}  // namespace boost

#endif  // __NETWORK_MESSAGE_TRANSFORMERS_HPP__
