#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <cstdint>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/network/protocol/http/client/connection/connection_delegate.hpp>
#include <boost/network/support/is_default_string.hpp>
#include <boost/network/support/is_default_wstring.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

struct ssl_delegate : public connection_delegate,
                      public std::enable_shared_from_this<ssl_delegate> {
  ssl_delegate(boost::asio::io_service &service, bool always_verify_peer,
               optional<std::string> certificate_filename,
               optional<std::string> verify_path,
               optional<std::string> certificate_file,
               optional<std::string> private_key_file,
               optional<std::string> ciphers,
               optional<std::string> sni_hostname, long ssl_options);

  void connect(boost::asio::ip::tcp::endpoint &endpoint, std::string host,
               std::uint16_t source_port, optional<std::string> sni_hostname,
               std::function<void(boost::system::error_code const &)> handler) override;
  void write(
      boost::asio::streambuf &command_streambuf,
      std::function<void(boost::system::error_code const &, size_t)> handler) override;
  void read_some(
      boost::asio::mutable_buffers_1 const &read_buffer,
      std::function<void(boost::system::error_code const &, size_t)> handler) override;
  void disconnect() override;
  ~ssl_delegate() override;

 private:
  boost::asio::io_service &service_;
  optional<std::string> certificate_filename_;
  optional<std::string> verify_path_;
  optional<std::string> certificate_file_;
  optional<std::string> private_key_file_;
  optional<std::string> ciphers_;
  optional<std::string> sni_hostname_;
  long ssl_options_;
  std::unique_ptr<boost::asio::ssl::context> context_;
  std::unique_ptr<boost::asio::ip::tcp::socket> tcp_socket_;
  std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket &> > socket_;
  bool always_verify_peer_;

  ssl_delegate(ssl_delegate const &);     // = delete
  ssl_delegate &operator=(ssl_delegate);  // = delete

  void handle_connected(boost::system::error_code const &ec,
                        std::function<void(boost::system::error_code const &)> handler);
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_20110819
