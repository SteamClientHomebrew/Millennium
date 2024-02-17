
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_WRAPPERS_BODY_HPP__
#define BOOST_NETWORK_MESSAGE_WRAPPERS_BODY_HPP__

#include <boost/network/detail/wrapper_base.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/network/message_fwd.hpp>

namespace boost {
namespace network {

template <class Message>
struct body_range {
  typedef typename boost::iterator_range<
      typename Message::string_type::const_iterator> type;
};

namespace impl {
template <class Tag>
struct body_wrapper : public detail::wrapper_base<Tag, basic_message<Tag> > {
  typedef basic_message<Tag> message_type;
  typedef typename string<Tag>::type string_type;
  typedef detail::wrapper_base<Tag, basic_message<Tag> > wrapper_base;

  explicit body_wrapper(basic_message<Tag>& message_)
      : wrapper_base(message_) {};

  operator string_type() const {
    return string_type(wrapper_base::_message.body());
  };

  std::size_t size() const { return wrapper_base::_message.body().size(); }

  operator boost::iterator_range<
      typename boost::range_iterator<string_type>::type>() const {
    return boost::make_iterator_range(wrapper_base::_message.body());
  }

  typename string_type::const_iterator begin() const {
    return wrapper_base::_message.body().begin();
  }

  typename string_type::const_iterator end() const {
    return wrapper_base::_message.body().end();
  }
};

template <class Tag>
struct body_wrapper_const
    : public detail::wrapper_base_const<Tag, basic_message<Tag> > {
  typedef basic_message<Tag> message_type;
  typedef typename string<Tag>::type string_type;
  typedef detail::wrapper_base_const<Tag, basic_message<Tag> > wrapper_base;

  explicit body_wrapper_const(basic_message<Tag> const& message_)
      : wrapper_base(message_) {};

  operator string_type() const {
    return string_type(wrapper_base::_message.body());
  }

  std::size_t size() const { return wrapper_base::_message.body().size(); }

  operator boost::range_iterator<string_type>() const {
    return boost::make_iterator_range(wrapper_base::_message.body());
  }
};

template <class Tag>
inline std::ostream& operator<<(std::ostream& os,
                                body_wrapper<Tag> const& body) {
  os << static_cast<typename body_wrapper<Tag>::string_type>(body);
  return os;
}

template <class Tag>
inline std::ostream& operator<<(std::ostream& os,
                                body_wrapper_const<Tag> const& body) {
  os << static_cast<typename body_wrapper_const<Tag>::string_type>(body);
  return os;
}

}  // namespace impl

template <class Tag>
inline impl::body_wrapper<Tag> const body(basic_message<Tag>& message_) {
  return impl::body_wrapper<Tag>(message_);
}

template <class Tag>
inline impl::body_wrapper_const<Tag> const body(
    basic_message<Tag> const& message_) {
  return impl::body_wrapper_const<Tag>(message_);
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_WRAPPERS_BODY_HPP__
