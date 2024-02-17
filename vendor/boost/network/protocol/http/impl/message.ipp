// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_IPP

#include <boost/network/protocol/http/message.hpp>
#include <cstdio>
#include <cstdlib>

namespace boost {
namespace network {
namespace http {

// static member functions of boost::network::http::message

template <typename Tag>
typename message_impl<Tag>::string_type const message_impl<Tag>::url_decode(
    typename message_impl<Tag>::string_type const &str) {
  char decode_buf[3];
  typename message_impl<Tag>::string_type result;
  result.reserve(str.size());

  for (typename message_impl<Tag>::string_type::size_type pos = 0;
       pos < str.size(); ++pos) {
    switch (str[pos]) {
      case '+':
        // convert to space character
        result += ' ';
        break;
      case '%':
        // decode hexidecimal value
        if (pos + 2 < str.size()) {
          decode_buf[0] = str[++pos];
          decode_buf[1] = str[++pos];
          decode_buf[2] = '\0';
          result += static_cast<char>(strtol(decode_buf, nullptr, 16));
        } else {
          // recover from error by not decoding character
          result += '%';
        }
        break;
      default:
        // character does not need to be escaped
        result += str[pos];
    }
  };

  return result;
}

template <typename Tag>
typename message_impl<Tag>::string_type const message_impl<Tag>::url_encode(
    typename message_impl<Tag>::string_type const &str) {
  char encode_buf[4];
  typename message_impl<Tag>::string_type result;
  encode_buf[0] = '%';
  result.reserve(str.size());

  // character selection for this algorithm is based on the following url:
  // http://www.blooberry.com/indexdot/html/topics/urlencoding.htm

  for (typename message_impl<Tag>::string_type::size_type pos = 0;
       pos < str.size(); ++pos) {
    switch (str[pos]) {
      default:
        if (str[pos] >= 32 && str[pos] < 127) {
          // character does not need to be escaped
          result += str[pos];
          break;
        }
      // else pass through to next case

      case '$':
      case '&':
      case '+':
      case ',':
      case '/':
      case ':':
      case ';':
      case '=':
      case '?':
      case '@':
      case '"':
      case '<':
      case '>':
      case '#':
      case '%':
      case '{':
      case '}':
      case '|':
      case '\\':
      case '^':
      case '~':
      case '[':
      case ']':
      case '`':
        // the character needs to be encoded
        sprintf(encode_buf + 1, "%02X", str[pos]);
        result += encode_buf;
        break;
    }
  };

  return result;
}

template <typename Tag>
typename message_impl<Tag>::string_type const
message_impl<Tag>::make_query_string(
    typename query_container<Tag>::type const &query_params) {
  typename message_impl<Tag>::string_type query_string;
  for (typename query_container<Tag>::type::const_iterator i =
           query_params.begin();
       i != query_params.end(); ++i) {
    if (i != query_params.begin()) { query_string += '&';

}query_string += url_encode(i->first);
    query_string += '=';
    query_string += url_encode(i->second);
  }
  return query_string;
}

template <typename Tag>
typename message_impl<Tag>::string_type const
message_impl<Tag>::make_set_cookie_header(
    typename message_impl<Tag>::string_type const &name,
    typename message_impl<Tag>::string_type const &value,
    typename message_impl<Tag>::string_type const &path, bool const has_max_age,
    unsigned long const max_age) {
  typename message_impl<Tag>::string_type set_cookie_header(name);
  set_cookie_header += "=\"";
  set_cookie_header += value;
  set_cookie_header += "\"; Version=\"1\"";
  if (!path.empty()) {
    set_cookie_header += "; Path=\"";
    set_cookie_header += path;
    set_cookie_header += '\"';
  }
  if (has_max_age) {
    set_cookie_header += "; Max-Age=\"";
    set_cookie_header += std::to_string(max_age);
    set_cookie_header += '\"';
  }
  return set_cookie_header;
}

template <typename Tag>
bool message_impl<Tag>::base64_decode(
    const typename message_impl<Tag>::string_type &input,
    typename message_impl<Tag>::string_type &output) {
  static const char nop = -1;
  static const char decoding_data[] = {
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, 62,  nop,
      nop, nop, 63,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  nop, nop,
      nop, nop, nop, nop, nop, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
      10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
      25,  nop, nop, nop, nop, nop, nop, 26,  27,  28,  29,  30,  31,  32,  33,
      34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
      49,  50,  51,  nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop,
      nop};

  unsigned int input_length = input.size();
  const char *input_ptr = input.data();

  // allocate space for output string
  output.clear();
  output.reserve(((input_length + 2) / 3) * 4);

  // for each 4-bytes sequence from the input, extract 4 6-bits sequences
  // by droping first two bits
  // and regenerate into 3 8-bits sequence

  for (unsigned int i = 0; i < input_length; i++) {
    char base64code0;
    char base64code1;
    char base64code2 = 0;  // initialized to 0 to suppress warnings
    char base64code3;

    base64code0 = decoding_data[static_cast<int>(input_ptr[i])];
    if (base64code0 == nop) {  // non base64 character
      return false;
}
    if (!(++i < input_length)) {  // we need at least two input bytes for
                                // first byte output
      return false;
}
    base64code1 = decoding_data[static_cast<int>(input_ptr[i])];
    if (base64code1 == nop) {  // non base64 character
      return false;
}

    output += ((base64code0 << 2) | ((base64code1 >> 4) & 0x3));

    if (++i < input_length) {
      char c = input_ptr[i];
      if (c == '=') {  // padding , end of input
        BOOST_ASSERT((base64code1 & 0x0f) == 0);
        return true;
      }
      base64code2 = decoding_data[static_cast<int>(input_ptr[i])];
      if (base64code2 == nop) {  // non base64 character
        return false;
}

      output += ((base64code1 << 4) & 0xf0) | ((base64code2 >> 2) & 0x0f);
    }

    if (++i < input_length) {
      char c = input_ptr[i];
      if (c == '=') {  // padding , end of input
        BOOST_ASSERT((base64code2 & 0x03) == 0);
        return true;
      }
      base64code3 = decoding_data[static_cast<int>(input_ptr[i])];
      if (base64code3 == nop) {  // non base64 character
        return false;
}

      output += (((base64code2 << 6) & 0xc0) | base64code3);
    }
  }

  return true;
}

template <typename Tag>
bool message_impl<Tag>::base64_encode(
    typename message_impl<Tag>::string_type const &input,
    typename message_impl<Tag>::string_type &output) {
  static const char encoding_data[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  unsigned int input_length = input.size();
  const char *input_ptr = input.data();

  // allocate space for output string
  output.clear();
  output.reserve(((input_length + 2) / 3) * 4);

  // for each 3-bytes sequence from the input, extract 4 6-bits sequences
  // and encode using
  // encoding_data lookup table.
  // if input do not contains enough chars to complete 3-byte sequence,use
  // pad char '='
  for (unsigned int i = 0; i < input_length; i++) {
    int base64code0 = 0;
    int base64code1 = 0;
    int base64code2 = 0;
    int base64code3 = 0;

    base64code0 = (input_ptr[i] >> 2) & 0x3f;  // 1-byte 6 bits
    output += encoding_data[base64code0];
    base64code1 = (input_ptr[i] << 4) & 0x3f;  // 1-byte 2 bits +

    if (++i < input_length) {
      base64code1 |= (input_ptr[i] >> 4) & 0x0f;  // 2-byte 4 bits
      output += encoding_data[base64code1];
      base64code2 = (input_ptr[i] << 2) & 0x3f;  // 2-byte 4 bits +

      if (++i < input_length) {
        base64code2 |= (input_ptr[i] >> 6) & 0x03;  // 3-byte 2 bits
        base64code3 = input_ptr[i] & 0x3f;          // 3-byte 6 bits
        output += encoding_data[base64code2];
        output += encoding_data[base64code3];
      } else {
        output += encoding_data[base64code2];
        output += '=';
      }
    } else {
      output += encoding_data[base64code1];
      output += '=';
      output += '=';
    }
  }

  return true;
}

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_HPP
