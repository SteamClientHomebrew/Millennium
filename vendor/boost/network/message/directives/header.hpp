
//          Copyright Dean Michael Berris 2007-2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_DIRECTIVES_HEADER_HPP__
#define BOOST_NETWORK_MESSAGE_DIRECTIVES_HEADER_HPP__

#include <boost/network/support/is_async.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/or.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace boost {
namespace network {

namespace impl {

template <class KeyType, class ValueType>
struct header_directive {

  explicit header_directive(KeyType const& header_name,
                            ValueType const& header_value)
      : _header_name(header_name), _header_value(header_value) {};

  template <class Message>
  struct pod_directive {
    template <class T1, class T2>
    static void eval(Message const& message, T1 const& key, T2 const& value) {
      typedef typename Message::headers_container_type::value_type value_type;
      value_type value_ = {key, value};
      message.headers.insert(message.headers.end(), value_);
    }
  };

  template <class Message>
  struct normal_directive {
    template <class T1, class T2>
    static void eval(Message const& message, T1 const& key, T2 const& value) {
      typedef typename Message::headers_container_type::value_type value_type;
      message.add_header(value_type(key, value));
    }
  };

  template <class Message>
  struct directive_impl
    : mpl::if_<std::is_base_of<tags::pod, typename Message::tag>,
                 pod_directive<Message>, normal_directive<Message> >::type {};

  template <class Message>
  void operator()(Message const& msg) const {
    directive_impl<Message>::eval(msg, _header_name, _header_value);
  }

 private:
  KeyType const& _header_name;
  ValueType const& _header_value;
};

}  // namespace impl

template <class T1, class T2>
inline impl::header_directive<T1, T2> header(T1 const& header_name,
                                             T2 const& header_value) {
  return impl::header_directive<T1, T2>(header_name, header_value);
}
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_DIRECTIVES_HEADER_HPP__
