// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_PARSER_HPP
#define BOOST_NETWORK_PROTOCOL_HTTP_PARSER_HPP

#include <cstdint>
#include <string>
#include <boost/network/protocol/http/traits.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/network/message.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/noncopyable.hpp>

namespace boost {
namespace network {
namespace http {

// forward declarations used to finish HTTP requests
template <typename Tag>
class basic_request;

// forward declarations used to finish HTTP requests
template <typename Tag>
class basic_response;

/// an incremental HTTP 1.0/1.1 protocol parser
template <typename Tag, typename ParserTraits = parser_traits<Tag> >
class basic_parser : private boost::noncopyable {
 public:
  // import types from ParserTraits template
  typedef ParserTraits traits_type;
  typedef typename string<Tag>::type string_type;

  // default destructor
  virtual ~basic_parser() {}

  /**
   * creates a new HTTP protocol parser
   *
   * @param is_request if true, the message is parsed as an HTTP request;
   *                   if false, the message is parsed as an HTTP response
   */
  basic_parser(const bool is_request)
      : m_is_request(is_request),
        m_read_ptr(NULL),
        m_read_end_ptr(NULL),
        m_headers_parse_state(is_request ? PARSE_METHOD_START
                                         : PARSE_HTTP_VERSION_H),
        m_chunked_content_parse_state(PARSE_CHUNK_SIZE_START),
        m_status_code(0),
        m_bytes_last_read(0),
        m_bytes_total_read(0) {}

  /**
   * parses an HTTP message up to the end of the headers using bytes
   * available in the read buffer
   *
   * @param http_msg the HTTP message object to populate from parsing
   *
   * @return boost::tribool result of parsing:
   *                        false = message has an error,
   *                        true = finished parsing HTTP headers,
   *                        indeterminate = not yet finished parsing HTTP
   *headers
   */
  boost::tribool parse_http_headers(basic_message<Tag>& http_msg);

  /**
   * parses a chunked HTTP message-body using bytes available in the read
   *buffer
   *
   * @param chunk_buffers buffers to be populated from parsing chunked
   *content
   *
   * @return boost::tribool result of parsing:
   *                        false = message has an error,
   *                        true = finished parsing message,
   *                        indeterminate = message is not yet finished
   */
  boost::tribool parse_chunks(types::chunk_cache_t& chunk_buffers);

  /**
   * prepares the payload content buffer and consumes any content
   *remaining
   * in the parser's read buffer
   *
   * @param http_msg the HTTP message object to consume content for
   * @return unsigned long number of content bytes consumed, if any
   */
  std::size_t consume_content(basic_message<Tag>& http_msg);

  /**
   * consume the bytes available in the read buffer, converting them into
   * the next chunk for the HTTP message
   *
   * @param chunk_buffers buffers to be populated from parsing chunked
   *content
   * @return unsigned long number of content bytes consumed, if any
   */
  std::size_t consume_content_as_next_chunk(
      types::chunk_cache_t& chunk_buffers);

  /**
   * finishes parsing an HTTP request message (copies over request-only
   *data)
   *
   * @param http_request the HTTP request object to finish
   */
  void finish(basic_request<Tag>& http_request);

  /**
   * finishes an HTTP response message (copies over response-only data)
   *
   * @param http_request the HTTP response object to finish
   */
  void finish(basic_response<Tag>& http_response);

  /**
   * resets the location and size of the read buffer
   *
   * @param ptr pointer to the first bytes available to be read
   * @param len number of bytes available to be read
   */
  inline void set_read_buffer(const char* ptr, std::size_t len) {
    m_read_ptr = ptr;
    m_read_end_ptr = ptr + len;
  }

  /**
   * saves the current read position bookmark
   *
   * @param read_ptr points to the next character to be consumed in the
   *read_buffer
   * @param read_end_ptr points to the end of the read_buffer (last byte +
   *1)
   */
  inline void save_read_position(const char*& read_ptr,
                                 const char*& read_end_ptr) const {
    read_ptr = m_read_ptr;
    read_end_ptr = m_read_end_ptr;
  }

  /// resets the parser to its initial state
  inline void reset(void);

  /// returns true if there are no more bytes available in the read buffer
  inline bool eof(void) const {
    return m_read_ptr == NULL || m_read_ptr >= m_read_end_ptr;
  }

  /// returns the number of bytes read during the last parse operation
  inline std::size_t gcount(void) const { return m_bytes_last_read; }

  /// returns the total number of bytes read while parsing the HTTP
  /// message
  inline std::size_t bytes_read(void) const { return m_bytes_total_read; }

  /// returns the number of bytes available in the read buffer
  inline std::size_t bytes_available(void) const {
    return (eof() ? 0 : (m_read_end_ptr - m_read_ptr));
  }

 protected:
  /**
   * parse key-value pairs out of a url-encoded string
   * (i.e. this=that&a=value)
   *
   * @param params container for key-values string pairs
   * @param ptr points to the start of the encoded string
   * @param len length of the encoded string, in bytes
   *
   * @return bool true if successful
   */
  static bool parse_url_encoded(types::query_params& params, const char* ptr,
                                const std::size_t len);

  /**
   * parse key-value pairs out of a "Cookie" request header
   * (i.e. this=that; a=value)
   *
   * @param params container for key-values string pairs
   * @param cookie_header header string to be parsed
   *
   * @return bool true if successful
   */
  static bool parse_cookie_header(types::cookie_params& params,
                                  const string_type& cookie_header);

  /// true if the message is an HTTP request; false if it is an HTTP
  /// response
  const bool m_is_request;

  /// points to the next character to be consumed in the read_buffer
  const char* m_read_ptr;

  /// points to the end of the read_buffer (last byte + 1)
  const char* m_read_end_ptr;

 private:
  // returns true if the argument is a special character
  inline static bool is_special(int c) {
    switch (c) {
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ',':
      case ';':
      case ':':
      case '\\':
      case '"':
      case '/':
      case '[':
      case ']':
      case '?':
      case '=':
      case '{':
      case '}':
      case ' ':
      case '\t':
        return true;
      default:
        return false;
    }
  }

  // returns true if the argument is a character
  inline static bool is_char(int c) { return (c >= 0 && c <= 127); }

  // returns true if the argument is a control character
  inline static bool is_control(int c) {
    return ((c >= 0 && c <= 31) || c == 127);
  }

  // returns true if the argument is a digit
  inline static bool is_digit(int c) { return (c >= '0' && c <= '9'); }

  // returns true if the argument is a hexadecimal digit
  inline static bool is_hex_digit(int c) {
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));
  }

  /// state used to keep track of where we are in parsing the HTTP headers
  enum headers_parse_state_t {
    PARSE_METHOD_START,
    PARSE_METHOD,
    PARSE_URI_STEM,
    PARSE_URI_QUERY,
    PARSE_HTTP_VERSION_H,
    PARSE_HTTP_VERSION_T_1,
    PARSE_HTTP_VERSION_T_2,
    PARSE_HTTP_VERSION_P,
    PARSE_HTTP_VERSION_SLASH,
    PARSE_HTTP_VERSION_MAJOR_START,
    PARSE_HTTP_VERSION_MAJOR,
    PARSE_HTTP_VERSION_MINOR_START,
    PARSE_HTTP_VERSION_MINOR,
    PARSE_STATUS_CODE_START,
    PARSE_STATUS_CODE,
    PARSE_STATUS_MESSAGE,
    PARSE_EXPECTING_NEWLINE,
    PARSE_EXPECTING_CR,
    PARSE_HEADER_WHITESPACE,
    PARSE_HEADER_START,
    PARSE_HEADER_NAME,
    PARSE_SPACE_BEFORE_HEADER_VALUE,
    PARSE_HEADER_VALUE,
    PARSE_EXPECTING_FINAL_NEWLINE,
    PARSE_EXPECTING_FINAL_CR
  };

  /// state used to keep track of where we are in parsing chunked content
  enum chunked_content_parse_state_t {
    PARSE_CHUNK_SIZE_START,
    PARSE_CHUNK_SIZE,
    PARSE_EXPECTING_CR_AFTER_CHUNK_SIZE,
    PARSE_EXPECTING_LF_AFTER_CHUNK_SIZE,
    PARSE_CHUNK,
    PARSE_EXPECTING_CR_AFTER_CHUNK,
    PARSE_EXPECTING_LF_AFTER_CHUNK,
    PARSE_EXPECTING_FINAL_CR_AFTER_LAST_CHUNK,
    PARSE_EXPECTING_FINAL_LF_AFTER_LAST_CHUNK
  };

  /// the current state of parsing HTTP headers
  headers_parse_state_t m_headers_parse_state;

  /// the current state of parsing chunked content
  chunked_content_parse_state_t m_chunked_content_parse_state;

  /// Used for parsing the HTTP response status code
  std::uint16_t m_status_code;

  /// Used for parsing the HTTP response status message
  string_type m_status_message;

  /// Used for parsing the request method
  string_type m_method;

  /// Used for parsing the name of resource requested
  string_type m_resource;

  /// Used for parsing the query string portion of a URI
  string_type m_query_string;

  /// Used for parsing the name of HTTP headers
  string_type m_header_name;

  /// Used for parsing the value of HTTP headers
  string_type m_header_value;

  /// Used for parsing the chunk size
  string_type m_chunk_size_str;

  /// Used for parsing the current chunk
  std::vector<char> m_current_chunk;

  /// number of bytes in the chunk currently being parsed
  std::size_t m_size_of_current_chunk;

  /// number of bytes read so far in the chunk currently being parsed
  std::size_t m_bytes_read_in_current_chunk;

  /// number of bytes read during last parse operation
  std::size_t m_bytes_last_read;

  /// total number of bytes read while parsing the HTTP message
  std::size_t m_bytes_total_read;
};

/// typedef for the default HTTP protocol parser implementation
typedef basic_parser<tags::default_> parser;

};  // namespace http

};  // namespace network

};  // namespace boost

// import implementation file
#include <boost/network/protocol/http/impl/parser.ipp>

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_PARSER_HPP
