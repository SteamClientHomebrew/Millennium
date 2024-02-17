#ifndef BOOST_NETWORK_UTILS_BASE64_ENCODE_HPP
#define BOOST_NETWORK_UTILS_BASE64_ENCODE_HPP

#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <algorithm>
#include <iterator>
#include <string>

namespace boost {
namespace network {
namespace utils {

// Implements a BASE64 converter working on an iterator range.
// If the input sequence does not end at the three-byte boundary, the last
// encoded value part is remembered in an encoding state to be able to
// continue with the next chunk; the BASE64 encoding processes the input
// by byte-triplets.
//
// Summarized interface:
//
// struct state<Value>  {
//     bool empty () const;
//     void clear();
// }
//
// OutputIterator encode(InputIterator begin, InputIterator end,
//                       OutputIterator output, State & rest)
// OutputIterator encode_rest(OutputIterator output, State & rest)
// OutputIterator encode(InputRange const & input, OutputIterator output,
//                       State & rest)
// OutputIterator encode(char const * value, OutputIterator output,
//                       state<char> & rest)
// std::basic_string<Char> encode(InputRange const & value, State & rest)
// std::basic_string<Char> encode(char const * value, state<char> & rest)
//
// OutputIterator encode(InputIterator begin, InputIterator end,
//                       OutputIterator output)
// OutputIterator encode(InputRange const & input, OutputIterator output)
// OutputIterator encode(char const * value, OutputIterator output)
// std::basic_string<Char> encode(InputRange const & value)
// std::basic_string<Char> encode(char const * value) {
//
// See also http://libb64.sourceforge.net, which served as inspiration.
// See also http://tools.ietf.org/html/rfc4648 for the specification.

namespace base64 {

namespace detail {

// Picks a character from the output alphabet for another 6-bit value
// from the input sequence to encode.
template <typename Value>
char encode_value(Value value) {
  static char const encoding[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
      "+/";
  return encoding[static_cast<unsigned int>(value)];
}

}  // namespace detail

// Stores the state after processing the last chunk by the encoder.  If
// the
// chunk byte-length is not divisible by three, the last (incomplete)
// value
// quantum canot be encoded right away; it has to wait for the next
// chunk
// of octets which will be processed joined (as if the trailing rest
// from
// the previous one was at its beinning).
template <typename Value>
struct state {
  state() : triplet_index(0), last_encoded_value(0) {}

  state(state<Value> const& source)
      : triplet_index(source.triplet_index),
        last_encoded_value(source.last_encoded_value) {}

  bool empty() const { return triplet_index == 0; }

  void clear() {
    // indicate that no rest has been left in the last encoded value
    // and no padding is needed for the encoded output
    triplet_index = 0;
    // the last encoded value, which may have been left from the last
    // encoding step, must be zeroed too; it is important before the
    // next encoding begins, because it works as a cyclic buffer and
    // must start empty - with zero
    last_encoded_value = 0;
  }

 protected:
  // number of the octet in the incomplete quantum, which has been
  // processed the last time; 0 means that the previous quantum was
  // complete 3 octets, 1 that just one octet was avalable and 2 that
  // two octets were available
  unsigned char triplet_index;
  // the value made of the previously shifted and or-ed octets which
  // was not completely split to 6-bit codes, because the last quantum
  // did not stop on the boundary of three octets
  Value last_encoded_value;

  // encoding of an input chunk needs to read and update the state
  template <typename InputIterator, typename OutputIterator, typename State>
  friend OutputIterator encode(InputIterator begin, InputIterator end,
                               OutputIterator output, State& rest);

  // finishing the encoding needs to read and clear the state
  template <typename OutputIterator, typename State>
  friend OutputIterator encode_rest(OutputIterator output, State& rest);
};

// Encodes an input sequence to BASE64 writing it to the output iterator
// and stopping if the last input tree-octet quantum was not complete,
// in
// which case it stores the state for the later continuation, when
// another
// input chunk is ready for the encoding.  The encoding must be finished
// by calling the encode_rest after processing the last chunk.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result;
// std::back_insert_iterator<std::basic_string<Char> > appender(result);
// base64::state<unsigned char> rest;
// base64::encode(buffer.begin(), buffer.end(), appender, rest);
// ...
// base64::encode_rest(appender, rest);
template <typename InputIterator, typename OutputIterator, typename State>
OutputIterator encode(InputIterator begin, InputIterator end,
                      OutputIterator output, State& rest) {
  typedef typename iterator_value<InputIterator>::type value_type;
  // continue with the rest of the last chunk - 2 or 4 bits which
  // are already shifted to the left and need to be or-ed with the
  // continuing data up to the target 6 bits
  value_type encoded_value = rest.last_encoded_value;
  // if the previous chunk stopped at encoding the first (1) or the
  // second
  // (2) octet of the three-byte quantum, jump to the right place,
  // otherwise start the loop with an empty encoded value buffer
  switch (rest.triplet_index) {
    // this loop processes the input sequence of bit-octets by bits,
    // shifting the current_value (used as a cyclic buffer) left and
    // or-ing next bits there, while pulling the bit-sextets from the
    // high word of the current_value
    for (value_type current_value;;) {
      case 0:
        // if the input sequence is empty or reached its end at the
        // 3-byte boundary, finish with an empty encoding state
        if (begin == end) {
          rest.triplet_index = 0;
          // the last encoded value is not interesting - it would not
          // be used, because processing of the next chunk will start
          // at the 3-byte boundary
          rest.last_encoded_value = 0;
          return output;
        }
        // read the first octet from the current triplet
        current_value = *begin++;
        // use just the upper 6 bits to encode it to the target alphabet
        encoded_value = (current_value & 0xfc) >> 2;
        *output++ = detail::encode_value(encoded_value);
        // shift the remaining two bits up to make place for the upoming
        // part of the next octet
        encoded_value = (current_value & 0x03) << 4;
      case 1:
        // if the input sequence reached its end after the first octet
        // from the quantum triplet, store the encoding state and finish
        if (begin == end) {
          rest.triplet_index = 1;
          rest.last_encoded_value = encoded_value;
          return output;
        }
        // read the second first octet from the current triplet
        current_value = *begin++;
        // combine the upper four bits (as the lower part) with the
        // previous two bits to encode it to the target alphabet
        encoded_value |= (current_value & 0xf0) >> 4;
        *output++ = detail::encode_value(encoded_value);
        // shift the remaining four bits up to make place for the
        // upoming
        // part of the next octet
        encoded_value = (current_value & 0x0f) << 2;
      case 2:
        // if the input sequence reached its end after the second octet
        // from the quantum triplet, store the encoding state and finish
        if (begin == end) {
          rest.triplet_index = 2;
          rest.last_encoded_value = encoded_value;
          return output;
        }
        // read the third octet from the current triplet
        current_value = *begin++;
        // combine the upper two bits (as the lower part) with the
        // previous four bits to encode it to the target alphabet
        encoded_value |= (current_value & 0xc0) >> 6;
        *output++ = detail::encode_value(encoded_value);
        // encode the remaining 6 bits to the target alphabet
        encoded_value = current_value & 0x3f;
        *output++ = detail::encode_value(encoded_value);
    }
  }
  return output;
}

// Finishes encoding of the previously processed chunks.  If their total
// byte-length was divisible by three, nothing is needed, if not, the
// last
// quantum will be encoded as if padded with zeroes, which will be
// indicated
// by appending '=' characters to the output.  This method must be
// always
// used at the end of encoding, if the previous chunks were encoded by
// the
// method overload accepting the encoding state.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result;
// std::back_insert_iterator<std::basic_string<Char> > appender(result);
// base64::state<unsigned char> rest;
// base64::encode(buffer.begin(), buffer.end(), appender, rest);
// ...
// base64::encode_rest(appender, rest);
template <typename OutputIterator, typename State>
OutputIterator encode_rest(OutputIterator output, State& rest) {
  if (!rest.empty()) {
    // process the last part of the trailing octet (either 4 or 2 bits)
    // as if the input was padded with zeros - without or-ing the next
    // input value to it; it has been already shifted to the left
    *output++ = detail::encode_value(rest.last_encoded_value);
    // at least one padding '=' will be always needed - at least two
    // bits are missing in the finally encoded 6-bit value
    *output++ = '=';
    // if the last octet was the first in the triplet (the index was
    // 1), four bits are missing in the finally encoded 6-bit value;
    // another '=' character is needed for the another two bits
    if (rest.triplet_index < 2) *output++ = '=';
    // clear the state all the time to make sure that another call to
    // the encode_rest would not cause damage; the last encoded value,
    // which may have been left there, must be zeroed too; it is
    // important before the next encoding begins, because it works as
    // a cyclic buffer and must start empty - with zero
    rest.clear();
  }
  return output;
}

// Encodes a part of an input sequence specified by the pair of begin
// and
// end iterators.to BASE64 writing it to the output iterator. If its
// total
// byte-length was not divisible by three, the output will be padded by
// the
// '=' characters.  If you encode an input consisting of mutiple chunks,
// use the method overload maintaining the encoding state.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result;
// base64::encode(buffer.begin(), buffer.end(),
// std::back_inserter(result));
template <typename InputIterator, typename OutputIterator>
OutputIterator encode(InputIterator begin, InputIterator end,
                      OutputIterator output) {
  state<typename iterator_value<InputIterator>::type> rest;
  output = encode(begin, end, output, rest);
  return encode_rest(output, rest);
}

// Encodes an entire input sequence to BASE64, which either supports
// begin()
// and end() methods returning boundaries of the sequence or the
// boundaries
// can be computed by the Boost::Range, writing it to the output
// iterator
// and stopping if the last input tree-octet quantum was not complete,
// in
// which case it stores the state for the later continuation, when
// another
// input chunk is ready for the encoding.  The encoding must be finished
// by calling the encode_rest after processing the last chunk.
//
// Warning: Buffers identified by C-pointers are processed including
// their
// termination character, if they have any.  This is unexpected at least
// for the storing literals, which have a specialization here to avoid
// it.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result;
// std::back_insert_iterator<std::basic_string<Char> > appender(result);
// base64::state<unsigned char> rest;
// base64::encode(buffer, appender, rest);
// ...
// base64::encode_rest(appender, rest);
template <typename InputRange, typename OutputIterator, typename State>
OutputIterator encode(InputRange const& input, OutputIterator output,
                      State& rest) {
  return encode(std::begin(input), std::end(input), output, rest);
}

// Encodes an entire string literal to BASE64, writing it to the output
// iterator and stopping if the last input tree-octet quantum was not
// complete, in which case it stores the state for the later
// continuation,
// when another input chunk is ready for the encoding.  The encoding
// must
// be finished by calling the encode_rest after processing the last
// chunk.
//
// The string literal is encoded without processing its terminating zero
// character, which is the usual expectation.
//
// std::basic_string<Char> result;
// std::back_insert_iterator<std::basic_string<Char> > appender(result);
// base64::state<char> rest;
// base64::encode("ab", appender, rest);
// ...
// base64::encode_rest(appender, rest);
template <typename OutputIterator>
OutputIterator encode(char const* value, OutputIterator output,
                      state<char>& rest) {
  return encode(value, value + strlen(value), output, rest);
}

// Encodes an entire input sequence to BASE64 writing it to the output
// iterator, which either supports begin() and end() methods returning
// boundaries of the sequence or the boundaries can be computed by the
// Boost::Range. If its total byte-length was not divisible by three,
// the output will be padded by the '=' characters.  If you encode an
// input consisting of mutiple chunks, use the method overload
// maintaining
// the encoding state.
//
// Warning: Buffers identified by C-pointers are processed including
// their
// termination character, if they have any.  This is unexpected at least
// for the storing literals, which have a specialization here to avoid
// it.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result;
// base64::encode(buffer, std::back_inserter(result));
template <typename InputRange, typename OutputIterator>
OutputIterator encode(InputRange const& value, OutputIterator output) {
  return encode(std::begin(value), std::end(value), output);
}

// Encodes an entire string literal to BASE64 writing it to the output
// iterator. If its total length (without the trailing zero) was not
// divisible by three, the output will be padded by the '=' characters.
// If you encode an input consisting of mutiple chunks, use the method
// overload maintaining the encoding state.
//
// The string literal is encoded without processing its terminating zero
// character, which is the usual expectation.
//
// std::basic_string<Char> result;
// base64::encode("ab", std::back_inserter(result));
template <typename OutputIterator>
OutputIterator encode(char const* value, OutputIterator output) {
  return encode(value, value + strlen(value), output);
}

// Encodes an entire input sequence to BASE64 returning the result as
// string, which either supports begin() and end() methods returning
// boundaries of the sequence or the boundaries can be computed by the
// Boost::Range. If its total byte-length was not divisible by three,
// the output will be padded by the '=' characters.  If you encode an
// input consisting of mutiple chunks, use other method maintaining
// the encoding state writing to an output iterator.
//
// Warning: Buffers identified by C-pointers are processed including
// their
// termination character, if they have any.  This is unexpected at least
// for the storing literals, which have a specialization here to avoid
// it.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_string<Char> result = base64::encode<Char>(buffer);
template <typename Char, typename InputRange>
std::basic_string<Char> encode(InputRange const& value) {
  std::basic_string<Char> result;
  encode(value, std::back_inserter(result));
  return result;
}

// Encodes an entire string literal to BASE64 returning the result as
// string. If its total byte-length was not divisible by three, the
// output will be padded by the '=' characters.  If you encode an
// input consisting of mutiple chunks, use other method maintaining
// the encoding state writing to an output iterator.
//
// The string literal is encoded without processing its terminating zero
// character, which is the usual expectation.
//
// std::basic_string<Char> result = base64::encode<Char>("ab");
template <typename Char>
std::basic_string<Char> encode(char const* value) {
  std::basic_string<Char> result;
  encode(value, std::back_inserter(result));
  return result;
}

// The function overloads for string literals encode the input without
// the terminating zero, which is usually expected, because the trailing
// zero byte is not considered a part of the string value; the overloads
// for an input range would wrap the string literal by Boost.Range and
// encode the full memory occupated by the string literal - including
// the
// unwanted last zero byte.

}  // namespace base64

}  // namespace utils
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_UTILS_BASE64_ENCODE_HPP
