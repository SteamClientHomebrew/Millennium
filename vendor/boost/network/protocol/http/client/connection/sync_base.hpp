#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_SYNC_CONNECTION_BASE_20091217
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_SYNC_CONNECTION_BASE_20091217

// Copyright 2013 Google, Inc.
// Copyright 2009 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <functional>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/traits/istringstream.hpp>
#include <boost/network/traits/ostringstream.hpp>
#include <boost/network/protocol/http/response.hpp>

#include <boost/network/protocol/http/client/connection/sync_normal.hpp>
#ifdef BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http/client/connection/sync_ssl.hpp>
#endif

namespace boost {
namespace network {
namespace http {
namespace impl {

template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_connection_base_impl {
 protected:
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename string<Tag>::type string_type;
  typedef std::function<typename resolver_base::resolver_iterator_pair(
      resolver_type&, string_type const&, string_type const&)>
      resolver_function_type;

  template <class Socket>
  void init_socket(Socket& socket_, resolver_type& resolver_,
                   string_type /*unused*/ const& hostname,
                   string_type const& port, resolver_function_type resolve_) {
    using boost::asio::ip::tcp;
    boost::system::error_code error = boost::asio::error::host_not_found;
    typename resolver_type::iterator endpoint_iterator, end;
    boost::tie(endpoint_iterator, end) = resolve_(resolver_, hostname, port);
    while (error && endpoint_iterator != end) {
      socket_.close();
      socket_.connect(tcp::endpoint(endpoint_iterator->endpoint().address(),
                                    endpoint_iterator->endpoint().port()),
                      error);
      ++endpoint_iterator;
    }

    if (error) throw boost::system::system_error(error);
  }

  template <class Socket>
  void read_status(Socket& socket_, basic_response<Tag>& response_,
                   boost::asio::streambuf& response_buffer) {
    boost::asio::read_until(socket_, response_buffer, "\r\n");
    std::istream response_stream(&response_buffer);
    string_type http_version;
    unsigned int status_code;
    string_type status_message;
    response_stream >> http_version >> status_code;
    std::getline(response_stream, status_message);
    trim_left(status_message);
    trim_right_if(status_message, boost::is_space() || boost::is_any_of("\r"));

    if (!response_stream || http_version.substr(0, 5) != "HTTP/")
      throw std::runtime_error("Invalid response");

    response_ << http::version(http_version) << http::status(status_code)
              << http::status_message(status_message);
  }

  template <class Socket>
  void read_headers(Socket& socket_, basic_response<Tag>& response_,
                    boost::asio::streambuf& response_buffer) {
    boost::asio::read_until(socket_, response_buffer, "\r\n\r\n");
    std::istream response_stream(&response_buffer);
    string_type header_line, name;
    while (std::getline(response_stream, header_line) && header_line != "\r") {
      trim_right_if(header_line, boost::is_space() || boost::is_any_of("\r"));
      typename string_type::size_type colon_offset;
      if (header_line.size() && header_line[0] == ' ') {
        assert(!name.empty());
        if (name.empty())
          throw std::runtime_error(std::string("Malformed header: ") +
                                   header_line);
        response_ << header(name, trim_left_copy(header_line));
      } else if ((colon_offset = header_line.find_first_of(':')) !=
                 string_type::npos) {
        name = header_line.substr(0, colon_offset);
        response_ << header(name, header_line.substr(colon_offset + 2));
      }
    }
  }

  template <class Socket>
  void send_request_impl(Socket& socket_, string_type /*unused*/ const& method,
                         boost::asio::streambuf& request_buffer) {
    // TODO(dberris): review parameter necessity.
    (void)method;

    write(socket_, request_buffer);
  }

  template <class Socket>
  void read_body_normal(Socket& socket_, basic_response<Tag>& response_,
                        boost::asio::streambuf& response_buffer,
                        typename ostringstream<Tag>::type& body_stream) {
    // TODO(dberris): review parameter necessity.
    (void)response_;

    boost::system::error_code error;
    if (response_buffer.size() > 0) body_stream << &response_buffer;

    while (boost::asio::read(socket_, response_buffer, boost::asio::transfer_at_least(1),
                      error)) {
      body_stream << &response_buffer;
    }
  }

  template <class Socket>
  void read_body_transfer_chunk_encoding(
      Socket& socket_, basic_response<Tag>& response_,
      boost::asio::streambuf& response_buffer,
      typename ostringstream<Tag>::type& body_stream) {
    boost::system::error_code error;
    // look for the content-length header
    typename headers_range<basic_response<Tag> >::type content_length_range =
        headers(response_)["Content-Length"];
    if (boost::empty(content_length_range)) {
      typename headers_range<basic_response<Tag> >::type
          transfer_encoding_range = headers(response_)["Transfer-Encoding"];
      if (boost::empty(transfer_encoding_range)) {
        read_body_normal(socket_, response_, response_buffer, body_stream);
        return;
      }
      if (boost::iequals(std::begin(transfer_encoding_range)->second,
                         "chunked")) {
        bool stopping = false;
        do {
          std::size_t chunk_size_line =
              read_until(socket_, response_buffer, "\r\n", error);
          if ((chunk_size_line == 0) && (error != boost::asio::error::eof))
            throw boost::system::system_error(error);
          std::size_t chunk_size = 0;
          string_type data;
          {
            std::istream chunk_stream(&response_buffer);
            std::getline(chunk_stream, data);
            typename istringstream<Tag>::type chunk_size_stream(data);
            chunk_size_stream >> std::hex >> chunk_size;
          }
          if (chunk_size == 0) {
            stopping = true;
            if (!read_until(socket_, response_buffer, "\r\n", error) &&
                (error != boost::asio::error::eof))
              throw boost::system::system_error(error);
          } else {
            bool stopping_inner = false;
            do {
              if (response_buffer.size() < (chunk_size + 2)) {
                std::size_t bytes_to_read =
                    (chunk_size + 2) - response_buffer.size();
                std::size_t chunk_bytes_read =
                    read(socket_, response_buffer,
                         boost::asio::transfer_at_least(bytes_to_read), error);
                if (chunk_bytes_read == 0) {
                  if (error != boost::asio::error::eof) throw boost::system::system_error(error);
                  stopping_inner = true;
                }
              }

              std::istreambuf_iterator<char> eos;
              std::istreambuf_iterator<char> stream_iterator(&response_buffer);
              for (; chunk_size > 0 && stream_iterator != eos; --chunk_size)
                body_stream << *stream_iterator++;
              response_buffer.consume(2);
            } while (!stopping_inner && chunk_size != 0);

            if (chunk_size != 0)
              throw std::runtime_error(
                  "Size mismatch between tranfer encoding chunk data "
                  "size and declared chunk size.");
          }
        } while (!stopping);
      } else
        throw std::runtime_error("Unsupported Transfer-Encoding.");
    } else {
      size_t already_read = response_buffer.size();
      if (already_read) body_stream << &response_buffer;
      size_t length =
          std::stoul(std::begin(content_length_range)->second) - already_read;
      if (length == 0) {
        return;
      }
      size_t bytes_read = 0;
      while ((bytes_read = boost::asio::read(socket_, response_buffer,
                                      boost::asio::transfer_at_least(1), error))) {
        body_stream << &response_buffer;
        length -= bytes_read;
        if ((length <= 0) || error) break;
      }
    }
  }

  template <class Socket>
  void read_body(Socket& socket_, basic_response<Tag>& response_,
                 boost::asio::streambuf& response_buffer) {
    typename ostringstream<Tag>::type body_stream;
    // TODO(dberris): tag dispatch based on whether it's HTTP 1.0 or HTTP 1.1
    if (version_major == 1 && version_minor == 0) {
      read_body_normal(socket_, response_, response_buffer, body_stream);
    } else if (version_major == 1 && version_minor == 1) {
      if (response_.version() == "HTTP/1.0") {
        read_body_normal(socket_, response_, response_buffer, body_stream);
      } else {
        read_body_transfer_chunk_encoding(socket_, response_, response_buffer,
                                          body_stream);
      }
    } else {
      throw std::runtime_error("Unsupported HTTP version number.");
    }

    response_ << network::body(body_stream.str());
  }
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct sync_connection_base {
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename string<Tag>::type string_type;
  typedef std::function<typename resolver_base::resolver_iterator_pair(
      resolver_type&, string_type const&, string_type const&)>
      resolver_function_type;
  typedef std::function<bool(string_type&)> body_generator_function_type;

  // FIXME make the certificate filename and verify path parameters be
  // optional
  // ranges
  static sync_connection_base<Tag, version_major, version_minor>*
      new_connection(
          resolver_type& resolver, resolver_function_type resolve, bool https,
          bool always_verify_peer, int timeout,
          optional<string_type> /*unused*/ const& certificate_filename =
              optional<string_type>(),
          optional<string_type> const& verify_path = optional<string_type>(),
          optional<string_type> const& certificate_file =
              optional<string_type>(),
          optional<string_type> const& private_key_file =
              optional<string_type>(),
          optional<string_type> const& ciphers = optional<string_type>(),
          optional<string_type> const& sni_hostname = optional<string_type>(),
          long ssl_options = 0) {
    if (https) {
#ifdef BOOST_NETWORK_ENABLE_HTTPS
      return dynamic_cast<
          sync_connection_base<Tag, version_major, version_minor>*>(
          new https_sync_connection<Tag, version_major, version_minor>(
              resolver, resolve, always_verify_peer, timeout,
              certificate_filename, verify_path, certificate_file,
              private_key_file, ciphers, sni_hostname, ssl_options));
#else
      throw std::runtime_error("HTTPS not supported.");
#endif
    }
    return dynamic_cast<
        sync_connection_base<Tag, version_major, version_minor>*>(
        new http_sync_connection<Tag, version_major, version_minor>(
            resolver, resolve, timeout));
  }

  virtual void init_socket(string_type const& hostname,
                           string_type const& port) = 0;
  virtual void send_request_impl(string_type const& method,
                                 basic_request<Tag> const& request_,
                                 body_generator_function_type generator) = 0;
  virtual void read_status(basic_response<Tag>& response_,
                           boost::asio::streambuf& response_buffer) = 0;
  virtual void read_headers(basic_response<Tag>& response_,
                            boost::asio::streambuf& response_buffer) = 0;
  virtual void read_body(basic_response<Tag>& response_,
                         boost::asio::streambuf& response_buffer) = 0;
  virtual bool is_open() = 0;
  virtual void close_socket() = 0;
  virtual ~sync_connection_base() = default;

 protected:
  sync_connection_base() = default;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_SYNC_CONNECTION_BASE_20091217
