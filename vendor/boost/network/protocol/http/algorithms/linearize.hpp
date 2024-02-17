#ifndef BOOST_NETWORK_PROTOCOL_HTTP_ALGORITHMS_LINEARIZE_HPP_20101028
#define BOOST_NETWORK_PROTOCOL_HTTP_ALGORITHMS_LINEARIZE_HPP_20101028

// Copyright 2010 Dean Michael Berris.
// Copyright 2014 Jussi Lyytinen
// Copyright 2015 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <boost/algorithm/string/compare.hpp>
#include <boost/concept/requires.hpp>
#include <boost/network/constants.hpp>
#include <boost/network/message/wrappers/body.hpp>
#include <boost/network/protocol/http/message/header/name.hpp>
#include <boost/network/protocol/http/message/header/value.hpp>
#include <boost/network/protocol/http/message/wrappers/headers.hpp>
#include <boost/network/protocol/http/message/wrappers/port.hpp>
#include <boost/network/protocol/http/traits/connection_keepalive.hpp>
#include <boost/network/traits/headers_container.hpp>
#include <boost/network/traits/ostringstream.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/version.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct linearize_header {
  typedef typename string<Tag>::type string_type;

  template <class Arguments>
  struct result;

  template <class This, class Arg>
  struct result<This(Arg)> {
    typedef string_type type;
  };

  template <class ValueType>
  string_type operator()(ValueType& header) {
    typedef typename ostringstream<Tag>::type output_stream;
    typedef constants<Tag> consts;
    output_stream header_line;
    header_line << name(header) << consts::colon() << consts::space()
                << value(header) << consts::crlf();
    return header_line.str();
  }
};

template <class Request, class OutputIterator>
OutputIterator linearize(Request const& request,
                         typename Request::string_type const& method,
                         unsigned version_major, unsigned version_minor,
                         OutputIterator oi) {
  typedef typename Request::tag Tag;
  typedef constants<Tag> consts;
  typedef typename string<Tag>::type string_type;
  static string_type http_slash = consts::http_slash(),
                     accept = consts::accept(),
                     accept_mime = consts::default_accept_mime(),
                     accept_encoding = consts::accept_encoding(),
                     default_accept_encoding =
                         consts::default_accept_encoding(),
                     crlf = consts::crlf(), host = consts::host(),
                     connection = consts::connection(), close = consts::close();
  boost::copy(method, oi);
  *oi = consts::space_char();
  if (request.path().empty() || request.path()[0] != consts::slash_char()) {
    *oi = consts::slash_char();
  }
  boost::copy(request.path(), oi);
  if (!request.query().empty()) {
    *oi = consts::question_mark_char();
    boost::copy(request.query(), oi);
  }
  if (!request.anchor().empty()) {
    *oi = consts::hash_char();
    boost::copy(request.anchor(), oi);
  }
  *oi = consts::space_char();
  boost::copy(http_slash, oi);
  string_type version_major_str = std::to_string(version_major),
              version_minor_str = std::to_string(version_minor);
  boost::copy(version_major_str, oi);
  *oi = consts::dot_char();
  boost::copy(version_minor_str, oi);
  boost::copy(crlf, oi);

  // We need to determine whether we've seen any of the following headers
  // before setting the defaults. We use a bitset to keep track of the
  // defaulted headers.
  enum { ACCEPT, ACCEPT_ENCODING, HOST, CONNECTION, MAX };
  std::bitset<MAX> found_headers;
  static char const* defaulted_headers[][2] = {
      {consts::accept(), consts::accept() + std::strlen(consts::accept())},
      {consts::accept_encoding(),
       consts::accept_encoding() + std::strlen(consts::accept_encoding())},
      {consts::host(), consts::host() + std::strlen(consts::host())},
      {consts::connection(),
       consts::connection() + std::strlen(consts::connection())}};

  typedef typename headers_range<Request>::type headers_range;
  typedef typename range_value<headers_range>::type headers_value;
  for (const headers_value& header : headers(request)) {
    string_type header_name = name(header), header_value = value(header);
    // Here we check that we have not seen an override to the defaulted
    // headers.
    for (int header_index = 0; header_index < MAX; ++header_index)
      if (std::distance(header_name.begin(), header_name.end()) ==
              std::distance(defaulted_headers[header_index][0],
                            defaulted_headers[header_index][1]) &&
          std::equal(header_name.begin(), header_name.end(),
                     defaulted_headers[header_index][0],
                     algorithm::is_iequal()))
        found_headers.set(header_index, true);

    // We ignore empty headers.
    if (header_value.empty()) continue;
    boost::copy(header_name, oi);
    *oi = consts::colon_char();
    *oi = consts::space_char();
    boost::copy(header_value, oi);
    boost::copy(crlf, oi);
  }

  if (!found_headers[HOST]) {
    boost::copy(host, oi);
    *oi = consts::colon_char();
    *oi = consts::space_char();
    boost::copy(request.host(), oi);
    boost::optional<std::uint16_t> port_ =
#if (_MSC_VER >= 1600 && BOOST_VERSION > 105500)
        port(request).as_optional();
#else
        port(request);
#endif
    if (port_) {
      string_type port_str = std::to_string(*port_);
      *oi = consts::colon_char();
      boost::copy(port_str, oi);
    }
    boost::copy(crlf, oi);
  }

  if (!found_headers[ACCEPT]) {
    boost::copy(accept, oi);
    *oi = consts::colon_char();
    *oi = consts::space_char();
    boost::copy(accept_mime, oi);
    boost::copy(crlf, oi);
  }

  if (version_major == 1u && version_minor == 1u &&
      !found_headers[ACCEPT_ENCODING]) {
    boost::copy(accept_encoding, oi);
    *oi = consts::colon_char();
    *oi = consts::space_char();
    boost::copy(default_accept_encoding, oi);
    boost::copy(crlf, oi);
  }

  if (!connection_keepalive<Tag>::value && !found_headers[CONNECTION]) {
    boost::copy(connection, oi);
    *oi = consts::colon_char();
    *oi = consts::space_char();
    boost::copy(close, oi);
    boost::copy(crlf, oi);
  }

  boost::copy(crlf, oi);
  auto body_data = body(request).range();
  return boost::copy(body_data, oi);
}

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_ALGORITHMS_LINEARIZE_HPP_20101028
