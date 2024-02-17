// This file is part of the Boost Network library
// Based on the Pion Network Library (r421)
// Copyright Atomic Labs, Inc. 2007-2008
// See http://cpp-netlib.sourceforge.net for library home page.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_PARSER_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_PARSER_IPP

#include <boost/network/protocol/http/parser.hpp>

namespace boost {
namespace network {
namespace http {

// member functions for class basic_parser<Tag,Traits>

template <typename Tag, typename ParserTraits>
boost::tribool basic_parser<Tag, Traits>::parse_http_headers(
    basic_message<Tag>& http_msg) {
  //
  // note that boost::tribool may have one of THREE states:
  //
  // false: encountered an error while parsing HTTP headers
  // true: finished successfully parsing the HTTP headers
  // indeterminate: parsed bytes, but the HTTP headers are not yet
  // finished
  //
  const char* read_start_ptr = m_read_ptr;
  m_bytes_last_read = 0;
  while (m_read_ptr < m_read_end_ptr) {

    switch (m_headers_parse_state) {
      case PARSE_METHOD_START:
        // we have not yet started parsing the HTTP method string
        if (*m_read_ptr != ' ' && *m_read_ptr != '\r' &&
            *m_read_ptr != '\n') {  // ignore leading whitespace
          if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
              is_special(*m_read_ptr))
            return false;
          m_headers_parse_state = PARSE_METHOD;
          m_method.erase();
          m_method.push_back(*m_read_ptr);
        }
        break;

      case PARSE_METHOD:
        // we have started parsing the HTTP method string
        if (*m_read_ptr == ' ') {
          m_resource.erase();
          m_headers_parse_state = PARSE_URI_STEM;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else if (m_method.size() >= ParserTraits::METHOD_MAX) {
          return false;
        } else {
          m_method.push_back(*m_read_ptr);
        }
        break;

      case PARSE_URI_STEM:
        // we have started parsing the URI stem (or resource name)
        if (*m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HTTP_VERSION_H;
        } else if (*m_read_ptr == '?') {
          m_query_string.erase();
          m_headers_parse_state = PARSE_URI_QUERY;
        } else if (is_control(*m_read_ptr)) {
          return false;
        } else if (m_resource.size() >= ParserTraits::RESOURCE_MAX) {
          return false;
        } else {
          m_resource.push_back(*m_read_ptr);
        }
        break;

      case PARSE_URI_QUERY:
        // we have started parsing the URI query string
        if (*m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HTTP_VERSION_H;
        } else if (is_control(*m_read_ptr)) {
          return false;
        } else if (m_query_string.size() >= ParserTraits::QUERY_STRING_MAX) {
          return false;
        } else {
          m_query_string.push_back(*m_read_ptr);
        }
        break;

      case PARSE_HTTP_VERSION_H:
        // parsing "HTTP"
        if (*m_read_ptr != 'H') return false;
        m_headers_parse_state = PARSE_HTTP_VERSION_T_1;
        break;

      case PARSE_HTTP_VERSION_T_1:
        // parsing "HTTP"
        if (*m_read_ptr != 'T') return false;
        m_headers_parse_state = PARSE_HTTP_VERSION_T_2;
        break;

      case PARSE_HTTP_VERSION_T_2:
        // parsing "HTTP"
        if (*m_read_ptr != 'T') return false;
        m_headers_parse_state = PARSE_HTTP_VERSION_P;
        break;

      case PARSE_HTTP_VERSION_P:
        // parsing "HTTP"
        if (*m_read_ptr != 'P') return false;
        m_headers_parse_state = PARSE_HTTP_VERSION_SLASH;
        break;

      case PARSE_HTTP_VERSION_SLASH:
        // parsing slash after "HTTP"
        if (*m_read_ptr != '/') return false;
        m_headers_parse_state = PARSE_HTTP_VERSION_MAJOR_START;
        break;

      case PARSE_HTTP_VERSION_MAJOR_START:
        // parsing the first digit of the major version number
        if (!is_digit(*m_read_ptr)) return false;
        http_msg.setVersionMajor(*m_read_ptr - '0');
        m_headers_parse_state = PARSE_HTTP_VERSION_MAJOR;
        break;

      case PARSE_HTTP_VERSION_MAJOR:
        // parsing the major version number (not first digit)
        if (*m_read_ptr == '.') {
          m_headers_parse_state = PARSE_HTTP_VERSION_MINOR_START;
        } else if (is_digit(*m_read_ptr)) {
          http_msg.setVersionMajor((http_msg.getVersionMajor() * 10) +
                                   (*m_read_ptr - '0'));
        } else {
          return false;
        }
        break;

      case PARSE_HTTP_VERSION_MINOR_START:
        // parsing the first digit of the minor version number
        if (!is_digit(*m_read_ptr)) return false;
        http_msg.setVersionMinor(*m_read_ptr - '0');
        m_headers_parse_state = PARSE_HTTP_VERSION_MINOR;
        break;

      case PARSE_HTTP_VERSION_MINOR:
        // parsing the major version number (not first digit)
        if (*m_read_ptr == ' ') {
          // should only happen for responses
          if (m_is_request) return false;
          m_headers_parse_state = PARSE_STATUS_CODE_START;
        } else if (*m_read_ptr == '\r') {
          // should only happen for requests
          if (!m_is_request) return false;
          m_headers_parse_state = PARSE_EXPECTING_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          // should only happen for requests
          if (!m_is_request) return false;
          m_headers_parse_state = PARSE_EXPECTING_CR;
        } else if (is_digit(*m_read_ptr)) {
          http_msg.setVersionMinor((http_msg.getVersionMinor() * 10) +
                                   (*m_read_ptr - '0'));
        } else {
          return false;
        }
        break;

      case PARSE_STATUS_CODE_START:
        // parsing the first digit of the response status code
        if (!is_digit(*m_read_ptr)) return false;
        m_status_code = (*m_read_ptr - '0');
        m_headers_parse_state = PARSE_STATUS_CODE;
        break;

      case PARSE_STATUS_CODE:
        // parsing the response status code (not first digit)
        if (*m_read_ptr == ' ') {
          m_status_message.erase();
          m_headers_parse_state = PARSE_STATUS_MESSAGE;
        } else if (is_digit(*m_read_ptr)) {
          m_status_code = ((m_status_code * 10) + (*m_read_ptr - '0'));
        } else {
          return false;
        }
        break;

      case PARSE_STATUS_MESSAGE:
        // parsing the response status message
        if (*m_read_ptr == '\r') {
          m_headers_parse_state = PARSE_EXPECTING_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          m_headers_parse_state = PARSE_EXPECTING_CR;
        } else if (is_control(*m_read_ptr)) {
          return false;
        } else if (m_status_message.size() >=
                   ParserTraits::STATUS_MESSAGE_MAX) {
          return false;
        } else {
          m_status_message.push_back(*m_read_ptr);
        }
        break;

      case PARSE_EXPECTING_NEWLINE:
        // we received a CR; expecting a newline to follow
        if (*m_read_ptr == '\n') {
          m_headers_parse_state = PARSE_HEADER_START;
        } else if (*m_read_ptr == '\r') {
          // we received two CR's in a row
          // assume CR only is (incorrectly) being used for line
          // termination
          // therefore, the message is finished
          ++m_read_ptr;
          m_bytes_last_read = (m_read_ptr - read_start_ptr);
          m_bytes_total_read += m_bytes_last_read;
          return true;
        } else if (*m_read_ptr == '\t' || *m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HEADER_WHITESPACE;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else {
          // assume it is the first character for the name of a header
          m_header_name.erase();
          m_header_name.push_back(*m_read_ptr);
          m_headers_parse_state = PARSE_HEADER_NAME;
        }
        break;

      case PARSE_EXPECTING_CR:
        // we received a newline without a CR
        if (*m_read_ptr == '\r') {
          m_headers_parse_state = PARSE_HEADER_START;
        } else if (*m_read_ptr == '\n') {
          // we received two newlines in a row
          // assume newline only is (incorrectly) being used for line
          // termination
          // therefore, the message is finished
          ++m_read_ptr;
          m_bytes_last_read = (m_read_ptr - read_start_ptr);
          m_bytes_total_read += m_bytes_last_read;
          return true;
        } else if (*m_read_ptr == '\t' || *m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HEADER_WHITESPACE;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else {
          // assume it is the first character for the name of a header
          m_header_name.erase();
          m_header_name.push_back(*m_read_ptr);
          m_headers_parse_state = PARSE_HEADER_NAME;
        }
        break;

      case PARSE_HEADER_WHITESPACE:
        // parsing whitespace before a header name
        if (*m_read_ptr == '\r') {
          m_headers_parse_state = PARSE_EXPECTING_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          m_headers_parse_state = PARSE_EXPECTING_CR;
        } else if (*m_read_ptr != '\t' && *m_read_ptr != ' ') {
          if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
              is_special(*m_read_ptr))
            return false;
          // assume it is the first character for the name of a header
          m_header_name.erase();
          m_header_name.push_back(*m_read_ptr);
          m_headers_parse_state = PARSE_HEADER_NAME;
        }
        break;

      case PARSE_HEADER_START:
        // parsing the start of a new header
        if (*m_read_ptr == '\r') {
          m_headers_parse_state = PARSE_EXPECTING_FINAL_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          m_headers_parse_state = PARSE_EXPECTING_FINAL_CR;
        } else if (*m_read_ptr == '\t' || *m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HEADER_WHITESPACE;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else {
          // first character for the name of a header
          m_header_name.erase();
          m_header_name.push_back(*m_read_ptr);
          m_headers_parse_state = PARSE_HEADER_NAME;
        }
        break;

      case PARSE_HEADER_NAME:
        // parsing the name of a header
        if (*m_read_ptr == ':') {
          m_header_value.erase();
          m_headers_parse_state = PARSE_SPACE_BEFORE_HEADER_VALUE;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else if (m_header_name.size() >= ParserTraits::HEADER_NAME_MAX) {
          return false;
        } else {
          // character (not first) for the name of a header
          m_header_name.push_back(*m_read_ptr);
        }
        break;

      case PARSE_SPACE_BEFORE_HEADER_VALUE:
        // parsing space character before a header's value
        if (*m_read_ptr == ' ') {
          m_headers_parse_state = PARSE_HEADER_VALUE;
        } else if (*m_read_ptr == '\r') {
          http_msg.addHeader(m_header_name, m_header_value);
          m_headers_parse_state = PARSE_EXPECTING_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          http_msg.addHeader(m_header_name, m_header_value);
          m_headers_parse_state = PARSE_EXPECTING_CR;
        } else if (!is_char(*m_read_ptr) || is_control(*m_read_ptr) ||
                   is_special(*m_read_ptr)) {
          return false;
        } else {
          // assume it is the first character for the value of a header
          m_header_value.push_back(*m_read_ptr);
          m_headers_parse_state = PARSE_HEADER_VALUE;
        }
        break;

      case PARSE_HEADER_VALUE:
        // parsing the value of a header
        if (*m_read_ptr == '\r') {
          http_msg.addHeader(m_header_name, m_header_value);
          m_headers_parse_state = PARSE_EXPECTING_NEWLINE;
        } else if (*m_read_ptr == '\n') {
          http_msg.addHeader(m_header_name, m_header_value);
          m_headers_parse_state = PARSE_EXPECTING_CR;
        } else if (is_control(*m_read_ptr)) {
          return false;
        } else if (m_header_value.size() >= ParserTraits::HEADER_VALUE_MAX) {
          return false;
        } else {
          // character (not first) for the value of a header
          m_header_value.push_back(*m_read_ptr);
        }
        break;

      case PARSE_EXPECTING_FINAL_NEWLINE:
        if (*m_read_ptr == '\n') ++m_read_ptr;
        m_bytes_last_read = (m_read_ptr - read_start_ptr);
        m_bytes_total_read += m_bytes_last_read;
        return true;

      case PARSE_EXPECTING_FINAL_CR:
        if (*m_read_ptr == '\r') ++m_read_ptr;
        m_bytes_last_read = (m_read_ptr - read_start_ptr);
        m_bytes_total_read += m_bytes_last_read;
        return true;
    }

    ++m_read_ptr;
  }

  m_bytes_last_read = (m_read_ptr - read_start_ptr);
  m_bytes_total_read += m_bytes_last_read;
  return boost::indeterminate;
}

template <typename Tag, typename ParserTraits>
boost::tribool basic_parser<Tag, Traits>::parse_chunks(
    types::chunk_cache_t& chunk_buffers) {
  //
  // note that boost::tribool may have one of THREE states:
  //
  // false: encountered an error while parsing message
  // true: finished successfully parsing the message
  // indeterminate: parsed bytes, but the message is not yet finished
  //
  const char* read_start_ptr = m_read_ptr;
  m_bytes_last_read = 0;
  while (m_read_ptr < m_read_end_ptr) {

    switch (m_chunked_content_parse_state) {
      case PARSE_CHUNK_SIZE_START:
        // we have not yet started parsing the next chunk size
        if (is_hex_digit(*m_read_ptr)) {
          m_chunk_size_str.erase();
          m_chunk_size_str.push_back(*m_read_ptr);
          m_chunked_content_parse_state = PARSE_CHUNK_SIZE;
        } else if (*m_read_ptr == ' ' || *m_read_ptr == '\x09' ||
                   *m_read_ptr == '\x0D' || *m_read_ptr == '\x0A') {
          // Ignore leading whitespace.  Technically, the standard
          // probably doesn't allow white space here,
          // but we'll be flexible, since there's no ambiguity.
          break;
        } else {
          return false;
        }
        break;

      case PARSE_CHUNK_SIZE:
        if (is_hex_digit(*m_read_ptr)) {
          m_chunk_size_str.push_back(*m_read_ptr);
        } else if (*m_read_ptr == '\x0D') {
          m_chunked_content_parse_state = PARSE_EXPECTING_LF_AFTER_CHUNK_SIZE;
        } else if (*m_read_ptr == ' ' || *m_read_ptr == '\x09') {
          // Ignore trailing tabs or spaces.  Technically, the standard
          // probably doesn't allow this,
          // but we'll be flexible, since there's no ambiguity.
          m_chunked_content_parse_state = PARSE_EXPECTING_CR_AFTER_CHUNK_SIZE;
        } else {
          return false;
        }
        break;

      case PARSE_EXPECTING_CR_AFTER_CHUNK_SIZE:
        if (*m_read_ptr == '\x0D') {
          m_chunked_content_parse_state = PARSE_EXPECTING_LF_AFTER_CHUNK_SIZE;
        } else if (*m_read_ptr == ' ' || *m_read_ptr == '\x09') {
          // Ignore trailing tabs or spaces.  Technically, the standard
          // probably doesn't allow this,
          // but we'll be flexible, since there's no ambiguity.
          break;
        } else {
          return false;
        }
        break;

      case PARSE_EXPECTING_LF_AFTER_CHUNK_SIZE:
        // We received a CR; expecting LF to follow.  We can't be flexible
        // here because
        // if we see anything other than LF, we can't be certain where the
        // chunk starts.
        if (*m_read_ptr == '\x0A') {
          m_bytes_read_in_current_chunk = 0;
          m_size_of_current_chunk = strtol(m_chunk_size_str.c_str(), 0, 16);
          if (m_size_of_current_chunk == 0) {
            m_chunked_content_parse_state =
                PARSE_EXPECTING_FINAL_CR_AFTER_LAST_CHUNK;
          } else {
            m_current_chunk.clear();
            m_chunked_content_parse_state = PARSE_CHUNK;
          }
        } else {
          return false;
        }
        break;

      case PARSE_CHUNK:
        if (m_bytes_read_in_current_chunk < m_size_of_current_chunk) {
          m_current_chunk.push_back(*m_read_ptr);
          m_bytes_read_in_current_chunk++;
        }
        if (m_bytes_read_in_current_chunk == m_size_of_current_chunk) {
          chunk_buffers.push_back(m_current_chunk);
          m_current_chunk.clear();
          m_chunked_content_parse_state = PARSE_EXPECTING_CR_AFTER_CHUNK;
        }
        break;

      case PARSE_EXPECTING_CR_AFTER_CHUNK:
        // we've read exactly m_size_of_current_chunk bytes since starting
        // the current chunk
        if (*m_read_ptr == '\x0D') {
          m_chunked_content_parse_state = PARSE_EXPECTING_LF_AFTER_CHUNK;
        } else {
          return false;
        }
        break;

      case PARSE_EXPECTING_LF_AFTER_CHUNK:
        // we received a CR; expecting LF to follow
        if (*m_read_ptr == '\x0A') {
          m_chunked_content_parse_state = PARSE_CHUNK_SIZE_START;
        } else {
          return false;
        }
        break;

      case PARSE_EXPECTING_FINAL_CR_AFTER_LAST_CHUNK:
        // we've read the final chunk; expecting final CRLF
        if (*m_read_ptr == '\x0D') {
          m_chunked_content_parse_state =
              PARSE_EXPECTING_FINAL_LF_AFTER_LAST_CHUNK;
        } else {
          return false;
        }
        break;

      case PARSE_EXPECTING_FINAL_LF_AFTER_LAST_CHUNK:
        // we received the final CR; expecting LF to follow
        if (*m_read_ptr == '\x0A') {
          ++m_read_ptr;
          m_bytes_last_read = (m_read_ptr - read_start_ptr);
          m_bytes_total_read += m_bytes_last_read;
          return true;
        } else {
          return false;
        }
    }

    ++m_read_ptr;
  }

  m_bytes_last_read = (m_read_ptr - read_start_ptr);
  m_bytes_total_read += m_bytes_last_read;
  return boost::indeterminate;
}

template <typename Tag, typename ParserTraits>
std::size_t basic_parser<Tag, Traits>::consume_content(
    basic_message<Tag>& http_msg) {
  // get the payload content length from the HTTP headers
  http_msg.updateContentLengthUsingHeader();

  // read the post content
  std::size_t content_bytes_to_read = http_msg.getContentLength();
  char* post_buffer = http_msg.createContentBuffer();

  if (m_read_ptr < m_read_end_ptr) {
    // there are extra bytes left from the last read operation
    // copy them into the beginning of the content buffer
    const std::size_t bytes_left_in_read_buffer = bytes_available();

    if (bytes_left_in_read_buffer >= http_msg.getContentLength()) {
      // the last read operation included all of the payload content
      memcpy(post_buffer, m_read_ptr, http_msg.getContentLength());
      content_bytes_to_read = 0;
      m_read_ptr += http_msg.getContentLength();
    } else {
      // only some of the post content has been read so far
      memcpy(post_buffer, m_read_ptr, bytes_left_in_read_buffer);
      content_bytes_to_read -= bytes_left_in_read_buffer;
      m_read_ptr = m_read_end_ptr;
    }
  }

  m_bytes_last_read = (http_msg.getContentLength() - content_bytes_to_read);
  m_bytes_total_read += m_bytes_last_read;
  return m_bytes_last_read;
}

template <typename Tag, typename ParserTraits>
std::size_t basic_parser<Tag, Traits>::consume_content_as_next_chunk(
    types::chunk_cache_t& chunk_buffers) {
  if (bytes_available() == 0) {
    m_bytes_last_read = 0;
  } else {
    std::vector<char> next_chunk;
    while (m_read_ptr < m_read_end_ptr) {
      next_chunk.push_back(*m_read_ptr);
      ++m_read_ptr;
    }
    chunk_buffers.push_back(next_chunk);
    m_bytes_last_read = next_chunk.size();
    m_bytes_total_read += m_bytes_last_read;
  }
  return m_bytes_last_read;
}

template <typename Tag, typename ParserTraits>
void basic_parser<Tag, Traits>::finish(basic_request<Tag>& http_request) {
  http_request.setIsValid(true);
  http_request.setMethod(m_method);
  http_request.setResource(m_resource);
  http_request.setQueryString(m_query_string);

  // parse query pairs from the URI query string
  if (!m_query_string.empty()) {
    if (!parseURLEncoded(http_request.getQueryParams(), m_query_string.c_str(),
                         m_query_string.size()))
  }

  // parse query pairs from post content (x-www-form-urlencoded)
  if (http_request.getHeader(types::HEADER_CONTENT_TYPE) ==
      types::CONTENT_TYPE_URLENCODED) {
    if (!parseURLEncoded(http_request.getQueryParams(),
                         http_request.getContent(),
                         http_request.getContentLength()))
  }

  // parse "Cookie" headers
  std::pair<types::headers::const_iterator, types::headers::const_iterator>
      cookie_pair = http_request.getHeaders().equal_range(types::HEADER_COOKIE);
  for (types::headers::const_iterator cookie_iterator = cookie_pair.first;
       cookie_iterator != http_request.getHeaders().end() &&
           cookie_iterator != cookie_pair.second;
       ++cookie_iterator) {
    if (!parseCookieHeader(http_request.getCookieParams(),
                           cookie_iterator->second))
  }
}

template <typename Tag, typename ParserTraits>
void basic_parser<Tag, Traits>::finish(basic_response<Tag>& http_response) {
  http_response.setIsValid(true);
  http_response.setStatusCode(m_status_code);
  http_response.setStatusMessage(m_status_message);
}

template <typename Tag, typename ParserTraits>
inline void basic_parser<Tag, Traits>::reset(void) {
  m_headers_parse_state =
      (m_is_request ? PARSE_METHOD_START : PARSE_HTTP_VERSION_H);
  m_chunked_content_parse_state = PARSE_CHUNK_SIZE_START;
  m_status_code = 0;
  m_status_message.erase();
  m_method.erase();
  m_resource.erase();
  m_query_string.erase();
  m_current_chunk.clear();
  m_bytes_last_read = m_bytes_total_read = 0;
}

template <typename Tag, typename ParserTraits>
static bool basic_parser<Tag, Traits>::parse_url_encoded(
    types::query_params& params, const char* ptr, const std::size_t len) {
  // used to track whether we are parsing the name or value
  enum query_parse_state_t {
    QUERY_PARSE_NAME,
    QUERY_PARSE_VALUE
  } parse_state = QUERY_PARSE_NAME;

  // misc other variables used for parsing
  const char* const end = ptr + len;
  string_type query_name;
  string_type query_value;

  // iterate through each encoded character
  while (ptr < end) {
    switch (parse_state) {

      case QUERY_PARSE_NAME:
        // parsing query name
        if (*ptr == '=') {
          // end of name found
          if (query_name.empty()) return false;
          parse_state = QUERY_PARSE_VALUE;
        } else if (*ptr == '&') {
          // value is empty (OK)
          if (query_name.empty()) return false;
          params.insert(std::make_pair(query_name, query_value));
          query_name.erase();
        } else if (is_control(*ptr) ||
                   query_name.size() >= ParserTraits::QUERY_NAME_MAX) {
          // control character detected, or max sized exceeded
          return false;
        } else {
          // character is part of the name
          query_name.push_back(*ptr);
        }
        break;

      case QUERY_PARSE_VALUE:
        // parsing query value
        if (*ptr == '&') {
          // end of value found (OK if empty)
          params.insert(std::make_pair(query_name, query_value));
          query_name.erase();
          query_value.erase();
          parse_state = QUERY_PARSE_NAME;
        } else if (is_control(*ptr) ||
                   query_value.size() >= ParserTraits::QUERY_VALUE_MAX) {
          // control character detected, or max sized exceeded
          return false;
        } else {
          // character is part of the value
          query_value.push_back(*ptr);
        }
        break;
    }

    ++ptr;
  }

  // handle last pair in string
  if (!query_name.empty())
    params.insert(std::make_pair(query_name, query_value));

  return true;
}

template <typename Tag, typename ParserTraits>
static bool basic_parser<Tag, Traits>::parse_cookie_header(
    types::cookie_params& params, const string_type& cookie_header) {
  // BASED ON RFC 2109
  //
  // The current implementation ignores cookie attributes which begin with
  // '$'
  // (i.e. $Path=/, $Domain=, etc.)

  // used to track what we are parsing
  enum cookie_parse_state_t {
    COOKIE_PARSE_NAME,
    COOKIE_PARSE_VALUE,
    COOKIE_PARSE_IGNORE
  } parse_state = COOKIE_PARSE_NAME;

  // misc other variables used for parsing
  string_type cookie_name;
  string_type cookie_value;
  char value_quote_character = '\0';

  // iterate through each character
  for (string_type::const_iterator string_iterator = cookie_header.begin();
       string_iterator != cookie_header.end(); ++string_iterator) {
    switch (parse_state) {

      case COOKIE_PARSE_NAME:
        // parsing cookie name
        if (*string_iterator == '=') {
          // end of name found
          if (cookie_name.empty()) return false;
          value_quote_character = '\0';
          parse_state = COOKIE_PARSE_VALUE;
        } else if (*string_iterator == ';' || *string_iterator == ',') {
          // ignore empty cookie names since this may occur naturally
          // when quoted values are encountered
          if (!cookie_name.empty()) {
            // value is empty (OK)
            if (cookie_name[0] != '$')
              params.insert(std::make_pair(cookie_name, cookie_value));
            cookie_name.erase();
          }
        } else if (*string_iterator != ' ') {  // ignore whitespace
          // check if control character detected, or max sized exceeded
          if (is_control(*string_iterator) ||
              cookie_name.size() >= ParserTraits::COOKIE_NAME_MAX)
            return false;
          // character is part of the name
          // cookie names are case insensitive -> convert to lowercase
          cookie_name.push_back(tolower(*string_iterator));
        }
        break;

      case COOKIE_PARSE_VALUE:
        // parsing cookie value
        if (value_quote_character == '\0') {
          // value is not (yet) quoted
          if (*string_iterator == ';' || *string_iterator == ',') {
            // end of value found (OK if empty)
            if (cookie_name[0] != '$')
              params.insert(std::make_pair(cookie_name, cookie_value));
            cookie_name.erase();
            cookie_value.erase();
            parse_state = COOKIE_PARSE_NAME;
          } else if (*string_iterator == '\'' || *string_iterator == '"') {
            if (cookie_value.empty()) {
              // begin quoted value
              value_quote_character = *string_iterator;
            } else if (cookie_value.size() >= ParserTraits::COOKIE_VALUE_MAX) {
              // max size exceeded
              return false;
            } else {
              // assume character is part of the (unquoted) value
              cookie_value.push_back(*string_iterator);
            }
          } else if (*string_iterator != ' ') {  // ignore unquoted whitespace
            // check if control character detected, or max sized exceeded
            if (is_control(*string_iterator) ||
                cookie_value.size() >= ParserTraits::COOKIE_VALUE_MAX)
              return false;
            // character is part of the (unquoted) value
            cookie_value.push_back(*string_iterator);
          }
        } else {
          // value is quoted
          if (*string_iterator == value_quote_character) {
            // end of value found (OK if empty)
            if (cookie_name[0] != '$')
              params.insert(std::make_pair(cookie_name, cookie_value));
            cookie_name.erase();
            cookie_value.erase();
            parse_state = COOKIE_PARSE_IGNORE;
          } else if (cookie_value.size() >= ParserTraits::COOKIE_VALUE_MAX) {
            // max size exceeded
            return false;
          } else {
            // character is part of the (quoted) value
            cookie_value.push_back(*string_iterator);
          }
        }
        break;

      case COOKIE_PARSE_IGNORE:
        // ignore everything until we reach a comma "," or semicolon ";"
        if (*string_iterator == ';' || *string_iterator == ',')
          parse_state = COOKIE_PARSE_NAME;
        break;
    }
  }

  // handle last cookie in string
  if (!cookie_name.empty() && cookie_name[0] != '$')
    params.insert(std::make_pair(cookie_name, cookie_value));

  return true;
}

};  // namespace http

};  // namespace network

};  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_PARSER_IPP
