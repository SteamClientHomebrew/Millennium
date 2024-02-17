#ifndef BOOST_NETWORK_UTILS_BASE64_ENCODE_IO_HPP
#define BOOST_NETWORK_UTILS_BASE64_ENCODE_IO_HPP

#include <boost/network/utils/base64/encode.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <iterator>

namespace boost {
namespace network {
namespace utils {

// Offers an interface to the BASE64 converter from encode.hpp, which is
// based on stream manipulators to be friendly to the usage with output
// streams combining heterogenous output by using the output operators.
// The encoding state is serialized to long and maintained in te
// extensible
// internal array of the output stream.
//
// Summarized interface - ostream manipulators and one function:
//
// encode(InputIterator begin, InputIterator end)
// encode(InputRange const & value)
// encode(char const * value)
// encode_rest
// clear_state
// bool empty_state(std::basic_ostream<Char> & output)

namespace base64 {
namespace io {

// force using the ostream_iterator from boost::archive to write wide
// characters reliably, althoth wchar_t may not be a native character
// type
using namespace boost::archive::iterators;

namespace detail {

// Enables transferring of the input sequence for the BASE64
// encoding into
// the ostream operator defined for it, which performs the
// operation.  It
// is to be used from ostream manipulators.
//
// std::basic_ostream<Char> & output = ...;
// output << input_wrapper<InputIterator>(value.begin(),
// value.end());
template <typename InputIterator>
struct input_wrapper {
  input_wrapper(InputIterator begin, InputIterator end)
      : begin(begin), end(end) {}

 private:
  InputIterator begin, end;

  // only encoding of an input sequence needs access to it
  template <typename Char, typename InputIterator2>
  friend std::basic_ostream<Char> &operator<<(
      std::basic_ostream<Char> &output,
      input_wrapper<InputIterator2> input_wrapper);
};

// The output stream state should be used only in a single scope
// around
// encoding operations.  Constructor performs initialization from
// the
// output stream internal extensible array and destructor updates it
// according to the encoding result.  It inherits from the
// base64::state
// to gain access to its protected members and allow easy value
// passing
// to base64::encode.
//
//
// std::basic_ostream<Char> & output = ...;
// {
//     state<Char, Value> rest(output);
//     base64::encode(..., ostream_iterator<Char>(output), rest);
// }
template <typename Char, typename Value>
struct state : public boost::network::utils::base64::state<Value> {
  typedef boost::network::utils::base64::state<Value> super_t;

  // initialize the (inherited) contents of the base64::state<> from
  // the
  // output stream internal extensible array
  state(std::basic_ostream<Char> &output) : super_t(), output(output) {
    const unsigned triplet_index_size = sizeof(super_t::triplet_index) * 8;
    unsigned long data = static_cast<unsigned long>(storage());
    // mask the long value with the bit-size of the triplet_index
    // member
    super_t::triplet_index = data & ((1 << triplet_index_size) - 1);
    // shift the long value right to remove the the triplet_index
    // value;
    // masking is not necessary because the last_encoded_value it is
    // the
    // last record stored in the data value
    super_t::last_encoded_value = data >> triplet_index_size;
  }

  // update the value in the output stream internal extensible array
  // by
  // the last (inherited) contents of the base64::state<>
  ~state() {
    const unsigned triplet_index_size = sizeof(super_t::triplet_index) * 8;
    // store the last_encoded_value in the data value first
    unsigned long data =
        static_cast<unsigned long>(super_t::last_encoded_value);
    // shift the long data value left to make place for storing the
    // full triplet_index value there
    data <<= triplet_index_size;
    data |= static_cast<unsigned long>(super_t::triplet_index);
    storage() = static_cast<long>(data);
  }

 private:
  // all data of the base64::state<> must be serializable into a
  // long
  // value allocated in the output stream internal extensible array
  BOOST_STATIC_ASSERT(sizeof(super_t) <= sizeof(long));

  // allow only the construction with an output stream (strict RAII)
  state();
  state(state<Char, Value> const &);

  std::basic_ostream<Char> &output;

  long &storage() {
    static int index = std::ios_base::xalloc();
    return output.iword(index);
  }
};

// Output operator implementing the BASE64 encoding for an input
// sequence
// which was wrapped by the ostream manipulator; the state must be
// preserved
// because multiple sequences can be sent in the ouptut by this
// operator.
template <typename Char, typename InputIterator>
std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &output,
                                     input_wrapper<InputIterator> input) {
  typedef typename iterator_value<InputIterator>::type value_type;
  state<Char, value_type> rest(output);
  base64::encode(input.begin, input.end, ostream_iterator<Char>(output), rest);
  return output;
}

}  // namespace detail

// Encoding ostream manipulator for sequences specified by the pair of
// begin
// and end iterators.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode(buffer.begin(), buffer.end()) << ...
// <<
//           base64::io::encode_rest<unsigned char>;
template <typename InputIterator>
detail::input_wrapper<InputIterator> encode(InputIterator begin,
                                            InputIterator end) {
  return detail::input_wrapper<InputIterator>(begin, end);
}

// Encoding ostream manipulator processing whole sequences which
// either
// support begin() and end() methods returning boundaries of the
// sequence
// or the boundaries can be computed by the Boost::Range.
//
// Warning: Buffers identified by C-pointers are processed including
// their
// termination character, if they have any.  This is unexpected at
// least
// for the storing literals, which have a specialization here to avoid
// it.
//
// std::vector<unsigned char> buffer = ...;
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode(buffer) << ... <<
//           base64::io::encode_rest<unsigned char>;
template <typename InputRange>
detail::input_wrapper<typename boost::range_const_iterator<InputRange>::type>
encode(InputRange const &value) {
  typedef typename boost::range_const_iterator<InputRange>::type InputIterator;
  return detail::input_wrapper<InputIterator>(std::begin(value),
                                              std::end(value));
}

// Encoding ostream manipulator processing string literals; the usual
// expectation from their encoding is processing only the string
// content
// without the terminating zero character.
//
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode("ab") << ... <<
// base64::io::encode_rest<char>;
inline detail::input_wrapper<char const *> encode(char const *value) {
  return detail::input_wrapper<char const *>(value, value + strlen(value));
}

// Encoding ostream manipulator which finishes encoding of the
// previously
// processed chunks.  If their total byte-length was divisible by
// three,
// nothing is needed, if not, the last quantum will be encoded as if
// padded
// with zeroes, which will be indicated by appending '=' characters to
// the
// output.  This manipulator must be always used at the end of
// encoding,
// after previous usages of the encode manipulator.
//
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode("ab") << ... <<
// base64::io::encode_rest<char>;
template <typename Value, typename Char>
std::basic_ostream<Char> &encode_rest(std::basic_ostream<Char> &output) {
  detail::state<Char, Value> rest(output);
  base64::encode_rest(ostream_iterator<Char>(output), rest);
  return output;
}

// Clears the encoding state in the internal array of the output
// stream.
// Use it to re-use a state object in an unknown state only;  Encoding
// of
// the last chunk must be followed by encode_rest otherwise the end of
// the
// input sequence may be missing in the encoded output.  The
// encode_rest
// ensures that the rest of the input sequence will be encoded
// corectly and
// the '=' padding applied as necessary.  The encode rest clears the
// state
// when finished.
//
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode("ab") << ...;
// output << clear_state<char>;
template <typename Value, typename Char>
std::basic_ostream<Char> &clear_state(std::basic_ostream<Char> &output) {
  detail::state<Char, Value> rest(output);
  rest.clear();
  return output;
}

// Checks if the encoding state in the internal array of the output
// stream
// is empty.
//
// std::basic_ostream<Char> & output = ...;
// output << base64::io::encode("ab") << ...;
// bool is_complete = base64::io::empty_state<char>(output);
template <typename Value, typename Char>
bool empty_state(std::basic_ostream<Char> &output) {
  detail::state<Char, Value> rest(output);
  return rest.empty();
}

}  // namespace io
}  // namespace base64

}  // namespace utils
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_UTILS_BASE64_ENCODE_IO_HPP
