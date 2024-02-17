
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NETWORK_MESSAGE_DIRECTIVES_REMOVE_HEADER_HPP
#define NETWORK_MESSAGE_DIRECTIVES_REMOVE_HEADER_HPP

#include <boost/network/traits/string.hpp>
#include <string>

namespace boost {
namespace network {

template <class Tag>
struct basic_message;

namespace impl {
template <class T>
struct remove_header_directive {

  explicit remove_header_directive(T header_name)
      : header_name_(std::move(header_name)) {};

  template <class MessageTag>
  void operator()(basic_message<MessageTag>& msg) const {
    msg.headers().erase(header_name_);
  }

 private:
  mutable T header_name_;
};

}  // namespace impl

inline impl::remove_header_directive<std::string> remove_header(
    const std::string& header_name) {
  return impl::remove_header_directive<std::string>(header_name);
}

inline impl::remove_header_directive<std::wstring> remove_header(
    const std::wstring& header_name) {
  return impl::remove_header_directive<std::wstring>(header_name);
}

}  // namespace network
}  // namespace boost

#endif  // NETWORK_MESSAGE_DIRECTIVES_REMOVE_HEADER_HPP
