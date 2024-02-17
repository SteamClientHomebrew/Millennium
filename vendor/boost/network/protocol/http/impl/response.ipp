//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2009 Dean Michael Berris (mikhailberis@gmail.com)
// Copyright (c) 2009 Tarroo, Inc.
// Copyright (c) 2014 Jussi Lyytinen (jussi@lyytinen.org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Note: This implementation has significantly changed from the original example
// from a plain header file into a header-only implementation using C++
// templates
// to reduce the dependence on building an external library.
//

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_RESPONSE_RESPONSE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_RESPONSE_RESPONSE_IPP

#include <boost/asio/buffer.hpp>
#include <boost/network/traits/vector.hpp>
#include <boost/network/protocol/http/message/header.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/traits/string.hpp>

namespace boost {
namespace network {
namespace http {

/// A reply to be sent to a client.
template <>
struct basic_response<tags::http_server> {
  typedef tags::http_server tag;
  typedef response_header<tags::http_server>::type header_type;

  /*! The status of the reply. Represent all the status codes of HTTP v1.1
   * from http://tools.ietf.org/html/rfc2616#page-39 and
   * http://tools.ietf.org/html/rfc6585
   */
  enum status_type {
    continue_http = 100,
    switching_protocols = 101,
    ok = 200,
    created = 201,
    accepted = 202,
    non_authoritative_information = 203,
    no_content = 204,
    reset_content = 205,
    partial_content = 206,
    multiple_choices = 300,
    moved_permanently = 301,
    moved_temporarily = 302,  ///< \deprecated Not HTTP standard
    found = 302,
    see_other = 303,
    not_modified = 304,
    use_proxy = 305,
    temporary_redirect = 307,
    bad_request = 400,
    unauthorized = 401,
    payment_required = 402,
    forbidden = 403,
    not_found = 404,
    not_supported = 405,  ///< \deprecated Not HTTP standard
    method_not_allowed = 405,
    not_acceptable = 406,
    proxy_authentication_required = 407,
    request_timeout = 408,
    conflict = 409,
    gone = 410,
    length_required = 411,
    precondition_failed = 412,
    request_entity_too_large = 413,
    request_uri_too_large = 414,
    unsupported_media_type = 415,
    unsatisfiable_range = 416,  ///< \deprecated Not HTTP standard
    requested_range_not_satisfiable = 416,
    expectation_failed = 417,
    precondition_required = 428,
    too_many_requests = 429,
    request_header_fields_too_large = 431,
    internal_server_error = 500,
    not_implemented = 501,
    bad_gateway = 502,
    service_unavailable = 503,
    gateway_timeout = 504,
    http_version_not_supported = 505,
    space_unavailable = 507,
    network_authentication_required = 511
  } status;

  /// The headers to be included in the reply.
  typedef vector<tags::http_server>::apply<header_type>::type headers_vector;
  headers_vector headers;

  /// The content to be sent in the reply.
  typedef string<tags::http_server>::type string_type;
  string_type content;

  /// Convert the reply into a vector of buffers. The buffers do not own
  /// the
  /// underlying memory blocks, therefore the reply object must remain
  /// valid and
  /// not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers() {
    using boost::asio::const_buffer;
    using boost::asio::buffer;
    static const char name_value_separator[] = {':', ' '};
    static const char crlf[] = {'\r', '\n'};
    std::vector<const_buffer> buffers;
    buffers.push_back(to_buffer(status));
    for (auto & h : headers) {
      buffers.push_back(buffer(h.name));
      buffers.push_back(buffer(name_value_separator));
      buffers.push_back(buffer(h.value));
      buffers.push_back(buffer(crlf));
    }
    buffers.push_back(buffer(crlf));
    buffers.push_back(buffer(content));
    return buffers;
  }

  /// Get a stock reply.
  static basic_response<tags::http_server> stock_reply(status_type status) {
    return stock_reply(status, to_string(status));
  }

  /// Get a stock reply with custom plain text data.
  static basic_response<tags::http_server> stock_reply(status_type status,
                                                       string_type content) {
    basic_response<tags::http_server> rep;
    rep.status = status;
    rep.content = content;
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = std::to_string(rep.content.size());
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = "text/html";
    return rep;
  }

  /// Swap response objects
  void swap(basic_response<tags::http_server> &r) {
    using std::swap;
    swap(headers, r.headers);
    swap(content, r.content);
  }

 private:
  static string_type to_string(status_type status) {
    switch (status) {
      // 2xx Success
      case basic_response<tags::http_server>::ok:
        return "";
      case basic_response<tags::http_server>::created:
        return
          "<html>"
          "<head><title>Created</title></head>"
          "<body><h1>201 Created</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::accepted:
        return
          "<html>"
          "<head><title>Accepted</title></head>"
          "<body><h1>202 Accepted</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::non_authoritative_information:
        return
          "<html>"
          "<head><title>Non-Authoritative Information</title></head>"
          "<body><h1>203 Non-Authoritative Information</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::no_content:
        return
          "<html>"
          "<head><title>No Content</title></head>"
          "<body><h1>204 Content</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::reset_content:
        return
          "<html>"
          "<head><title>Reset Content</title></head>"
          "<body><h1>205 Reset Content</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::partial_content:
        return
          "<html>"
          "<head><title>Partial Content</title></head>"
          "<body><h1>206 Partial Content</h1></body>"
          "</html>";

      // 3xx Redirection
      case basic_response<tags::http_server>::multiple_choices:
        return
          "<html>"
          "<head><title>Multiple Choices</title></head>"
          "<body><h1>300 Multiple Choices</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::moved_permanently:
        return
          "<html>"
          "<head><title>Moved Permanently</title></head>"
          "<body><h1>301 Moved Permanently</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::moved_temporarily:
        return
          "<html>"
          "<head><title>Moved Temporarily</title></head>"
          "<body><h1>302 Moved Temporarily</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::see_other:
        return
          "<html>"
          "<head><title>See Other</title></head>"
          "<body><h1>303 See Other</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::not_modified:
        return
          "<html>"
          "<head><title>Not Modified</title></head>"
          "<body><h1>304 Not Modified</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::use_proxy:
        return
          "<html>"
          "<head><title>Use Proxy</title></head>"
          "<body><h1>305 Use Proxy</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::temporary_redirect:
        return
          "<html>"
          "<head><title>Temporary Redirect</title></head>"
          "<body><h1>307 Temporary Redirect</h1></body>"
          "</html>";

      // 4xx Client Error
      case basic_response<tags::http_server>::bad_request:
        return
          "<html>"
          "<head><title>Bad Request</title></head>"
          "<body><h1>400 Bad Request</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::unauthorized:
        return
          "<html>"
          "<head><title>Unauthorized</title></head>"
          "<body><h1>401 Unauthorized</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::forbidden:
        return
          "<html>"
          "<head><title>Forbidden</title></head>"
          "<body><h1>403 Forbidden</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::not_found:
        return
          "<html>"
          "<head><title>Not Found</title></head>"
          "<body><h1>404 Not Found</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::not_supported:
        return
          "<html>"
          "<head><title>Method Not Supported</title></head>"
          "<body><h1>405 Method Not Supported</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::not_acceptable:
        return
          "<html>"
          "<head><title>Not Acceptable\r\n</title></head>"
          "<body><h1>406 Not Acceptable</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::proxy_authentication_required:
        return
          "<html>"
          "<head><title>Proxy Authentication Required</title></head>"
          "<body><h1>407 Proxy Authentication Required</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::request_timeout:
        return
          "<html>"
          "<head><title>Request Timeout</title></head>"
          "<body><h1>408 Request Timeout</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::conflict:
        return
          "<html>"
          "<head><title>Conflict</title></head>"
          "<body><h1>409 Conflict</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::gone:
        return
          "<html>"
          "<head><title>Gone</title></head>"
          "<body><h1>410 Gone</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::length_required:
        return
          "<html>"
          "<head><title>Length Required</title></head>"
          "<body><h1>411 Length Required</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::precondition_failed:
        return
          "<html>"
          "<head><title>Precondition Failed</title></head>"
          "<body><h1>412 Precondition Failed</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::request_entity_too_large:
        return
          "<html>"
          "<head><title>Request Entity Too Large</title></head>"
          "<body><h1>413 Request Entity Too Large</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::request_uri_too_large:
        return
          "<html>"
          "<head><title>Request-URI Too Large</title></head>"
          "<body><h1>414 Request-URI Too Large</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::unsupported_media_type:
        return
          "<html>"
          "<head><title>Unsupported Media Type</title></head>"
          "<body><h1>415 Unsupported Media Type</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::unsatisfiable_range:
        return
          "<html>"
          "<head><title>Unsatisfiable Range</title></head>"
          "<body><h1>416 Requested Range Not "
          "Satisfiable</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::expectation_failed:
        return
          "<html>"
          "<head><title>Expectation Failed</title></head>"
          "<body><h1>417 Expectation Failed</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::precondition_required:
        return
          "<html>"
          "<head><title>Precondition Required</title></head>"
          "<body><h1>428 Precondition Required</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::too_many_requests:
        return
          "<html>"
          "<head><title>Too Many Requests</title></head>"
          "<body><h1>429 Too Many Requests</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::request_header_fields_too_large:
        return
          "<html>"
          "<head><title>Request Header Fields Too Large</title></head>"
          "<body><h1>431 Request Header Fields Too Large</h1></body>"
          "</html>";

      // 5xx Server Error
      case basic_response<tags::http_server>::internal_server_error:
        return
          "<html>"
          "<head><title>Internal Server Error</title></head>"
          "<body><h1>500 Internal Server Error</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::not_implemented:
        return
          "<html>"
          "<head><title>Not Implemented</title></head>"
          "<body><h1>501 Not Implemented</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::bad_gateway:
        return
          "<html>"
          "<head><title>Bad Gateway</title></head>"
          "<body><h1>502 Bad Gateway</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::service_unavailable:
        return
          "<html>"
          "<head><title>Service Unavailable</title></head>"
          "<body><h1>503 Service Unavailable</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::gateway_timeout:
        return
          "<html>"
          "<head><title>Gateway Timeout</title></head>"
          "<body><h1>504 Gateway Timeout</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::http_version_not_supported:
        return
          "<html>"
          "<head><title>HTTP Version Not Supported</title></head>"
          "<body><h1>505 HTTP Version Not Supported</h1></body>"
          "</html>";
      case basic_response<tags::http_server>::space_unavailable:
        return
          "<html>"
          "<head><title>Space Unavailable</title></head>"
          "<body><h1>507 Insufficient Space to Store "
          "Resource</h1></body>"
          "</html>";

      default:
        return
          "<html>"
          "<head><title>Internal Server Error</title></head>"
          "<body><h1>500 Internal Server Error</h1></body>"
          "</html>";
    }
  }

  boost::asio::const_buffer trim_null(boost::asio::const_buffer buffer) {
    std::size_t size = boost::asio::buffer_size(buffer);
    return boost::asio::buffer(buffer, size - 1);
  }

  boost::asio::const_buffer to_buffer(status_type status) {
    using boost::asio::buffer;
    switch (status) {
      // 2xx Success
      case basic_response<tags::http_server>::ok:
        return trim_null(buffer("HTTP/1.1 200 OK\r\n"));
      case basic_response<tags::http_server>::created:
        return trim_null(buffer("HTTP/1.1 201 Created\r\n"));
      case basic_response<tags::http_server>::accepted:
        return trim_null(buffer("HTTP/1.1 202 Accepted\r\n"));
      case basic_response<tags::http_server>::non_authoritative_information:
        return trim_null(buffer("HTTP/1.1 203 Non-Authoritative Information\r\n"));
      case basic_response<tags::http_server>::no_content:
        return trim_null(buffer("HTTP/1.1 204 No Content\r\n"));
      case basic_response<tags::http_server>::reset_content:
        return trim_null(buffer("HTTP/1.1 205 Reset Content\r\n"));
      case basic_response<tags::http_server>::partial_content:
        return trim_null(buffer("HTTP/1.1 206 Partial Content\r\n"));

      // 3xx Redirection
      case basic_response<tags::http_server>::multiple_choices:
        return trim_null(buffer("HTTP/1.1 300 Multiple Choices\r\n"));
      case basic_response<tags::http_server>::moved_permanently:
        return trim_null(buffer("HTTP/1.1 301 Moved Permanently\r\n"));
      case basic_response<tags::http_server>::moved_temporarily:
        return trim_null(buffer("HTTP/1.1 302 Moved Temporarily\r\n"));
      case basic_response<tags::http_server>::see_other:
        return trim_null(buffer("HTTP/1.1 303 See Other\r\n"));
      case basic_response<tags::http_server>::not_modified:
        return trim_null(buffer("HTTP/1.1 304 Not Modified\r\n"));
      case basic_response<tags::http_server>::use_proxy:
        return trim_null(buffer("HTTP/1.1 305 Use Proxy\r\n"));
      case basic_response<tags::http_server>::temporary_redirect:
        return trim_null(buffer("HTTP/1.1 307 Temporary Redirect\r\n"));

      // 4xx Client Error
      case basic_response<tags::http_server>::bad_request:
        return trim_null(buffer("HTTP/1.1 400 Bad Request\r\n"));
      case basic_response<tags::http_server>::unauthorized:
        return trim_null(buffer("HTTP/1.1 401 Unauthorized\r\n"));
      case basic_response<tags::http_server>::forbidden:
        return trim_null(buffer("HTTP/1.1 403 Forbidden\r\n"));
      case basic_response<tags::http_server>::not_found:
        return trim_null(buffer("HTTP/1.1 404 Not Found\r\n"));
      case basic_response<tags::http_server>::not_supported:
        return trim_null(buffer("HTTP/1.1 405 Method Not Supported\r\n"));
      case basic_response<tags::http_server>::not_acceptable:
        return trim_null(buffer("HTTP/1.1 406 Method Not Acceptable\r\n"));
      case basic_response<tags::http_server>::proxy_authentication_required:
        return trim_null(buffer("HTTP/1.1 407 Proxy Authentication Required\r\n"));
      case basic_response<tags::http_server>::request_timeout:
        return trim_null(buffer("HTTP/1.1 408 Request Timeout\r\n"));
      case basic_response<tags::http_server>::conflict:
        return trim_null(buffer("HTTP/1.1 409 Conflict\r\n"));
      case basic_response<tags::http_server>::gone:
        return trim_null(buffer("HTTP/1.1 410 Gone\r\n"));
      case basic_response<tags::http_server>::length_required:
        return trim_null(buffer("HTTP/1.1 411 Length Required\r\n"));
      case basic_response<tags::http_server>::precondition_failed:
        return trim_null(buffer("HTTP/1.1 412 Precondition Failed\r\n"));
      case basic_response<tags::http_server>::request_entity_too_large:
        return trim_null(buffer("HTTP/1.1 413 Request Entity Too Large\r\n"));
      case basic_response<tags::http_server>::request_uri_too_large:
        return trim_null(buffer("HTTP/1.1 414 Request-URI Too Large\r\n"));
      case basic_response<tags::http_server>::unsupported_media_type:
        return trim_null(buffer("HTTP/1.1 415 Unsupported Media Type\r\n"));
      case basic_response<tags::http_server>::unsatisfiable_range:
        return trim_null(buffer("HTTP/1.1 416 Requested Range Not Satisfiable\r\n"));
      case basic_response<tags::http_server>::precondition_required:
        return trim_null(buffer("HTTP/1.1 428 Precondition Required\r\n"));
      case basic_response<tags::http_server>::too_many_requests:
        return trim_null(buffer("HTTP/1.1 429 Too Many Requests\r\n"));
      case basic_response<tags::http_server>::request_header_fields_too_large:
        return trim_null(buffer("HTTP/1.1 431 Request Header Fields Too Large\r\n"));

      // 5xx Server Error
      case basic_response<tags::http_server>::internal_server_error:
        return trim_null(buffer("HTTP/1.1 500 Internal Server Error\r\n"));
      case basic_response<tags::http_server>::not_implemented:
        return trim_null(buffer("HTTP/1.1 501 Not Implemented\r\n"));
      case basic_response<tags::http_server>::bad_gateway:
        return trim_null(buffer("HTTP/1.1 502 Bad Gateway\r\n"));
      case basic_response<tags::http_server>::service_unavailable:
        return trim_null(buffer("HTTP/1.1 503 Service Unavailable\r\n"));
      case basic_response<tags::http_server>::gateway_timeout:
        return trim_null(buffer("HTTP/1.1 504 Gateway Timeout\r\n"));
      case basic_response<tags::http_server>::http_version_not_supported:
        return trim_null(buffer("HTTP/1.1 505 HTTP Version Not Supported\r\n"));
      case basic_response<tags::http_server>::space_unavailable:
        return trim_null(buffer("HTTP/1.1 507 Insufficient Space to Store Resource\r\n"));

      default:
        return trim_null(buffer("HTTP/1.1 500 Internal Server Error\r\n"));
    }
  }
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_RESPONSE_RESPONSE_IPP
