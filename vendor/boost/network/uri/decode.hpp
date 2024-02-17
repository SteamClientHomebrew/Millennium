//            Copyright (c) Glyn Matthews 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_DECODE_INC__
#define __BOOST_NETWORK_URI_DECODE_INC__

#include <iterator>
#include <boost/iterator/iterator_traits.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <cassert>
#include <stdexcept>
#include <string>

namespace boost {
namespace network {
namespace uri {
namespace detail {
template <typename CharT>
CharT letter_to_hex(CharT in) {
  switch (in) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return in - '0';
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      return in + 10 - 'a';
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
      return in + 10 - 'A';
  }
  return CharT();
}
}  // namespace detail

template <class InputIterator, class OutputIterator>
OutputIterator decode(const InputIterator &in_begin,
                      const InputIterator &in_end,
                      const OutputIterator &out_begin) {
  typedef typename boost::iterator_value<InputIterator>::type value_type;

  InputIterator it = in_begin;
  OutputIterator out = out_begin;
  while (it != in_end) {
    if (*it == '%') {
      if (++it == in_end) throw std::out_of_range("unexpected end of stream");
      value_type v0 = detail::letter_to_hex(*it);
      if (++it == in_end) throw std::out_of_range("unexpected end of stream");
      value_type v1 = detail::letter_to_hex(*it);
      ++it;
      *out++ = 0x10 * v0 + v1;
    } else if (*it == '+') {
      *out++ = ' ';
      ++it;
    } else {
      *out++ = *it++;
    }
  }
  return out;
}

template <class SinglePassRange, class OutputIterator>
inline OutputIterator decode(const SinglePassRange &range,
                             const OutputIterator &out) {
  return decode(std::begin(range), std::end(range), out);
}

inline std::string decoded(const std::string &input) {
  std::string decoded;
  decode(input, std::back_inserter(decoded));
  return decoded;
}

}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_URI_DECODE_INC__
