#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_OPTIONS_HPP_20130128
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_OPTIONS_HPP_20130128

// Copyright 2013 Google, Inc.
// Copyright 2013 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/optional/optional.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
class client_options {
 public:
  typedef typename string<Tag>::type string_type;

  /// Set all the options to default.
  client_options()
      : cache_resolved_(false),
        follow_redirects_(false),
        openssl_certificate_(),
        openssl_verify_path_(),
        openssl_certificate_file_(),
        openssl_private_key_file_(),
        openssl_ciphers_(),
        openssl_sni_hostname_(),
        openssl_options_(0),
        io_service_(),
        always_verify_peer_(true),
        timeout_(0),
        remove_chunk_markers_(true) {}

  client_options(client_options const& other)
      : cache_resolved_(other.cache_resolved_),
        follow_redirects_(other.follow_redirects_),
        openssl_certificate_(other.openssl_certificate_),
        openssl_verify_path_(other.openssl_verify_path_),
        openssl_certificate_file_(other.openssl_certificate_file_),
        openssl_private_key_file_(other.openssl_private_key_file_),
        openssl_ciphers_(other.openssl_ciphers_),
        openssl_sni_hostname_(other.openssl_sni_hostname_),
        openssl_options_(other.openssl_options_),
        io_service_(other.io_service_),
        always_verify_peer_(other.always_verify_peer_),
        timeout_(other.timeout_),
        remove_chunk_markers_(other.remove_chunk_markers_) {}

  client_options& operator=(client_options other) {
    other.swap(*this);
    return *this;
  }

  void swap(client_options& other) {
    using std::swap;
    swap(cache_resolved_, other.cache_resolved_);
    swap(follow_redirects_, other.follow_redirects_);
    swap(openssl_certificate_, other.openssl_certificate_);
    swap(openssl_verify_path_, other.openssl_verify_path_);
    swap(openssl_certificate_file_, other.openssl_certificate_file_);
    swap(openssl_private_key_file_, other.openssl_private_key_file_);
    swap(openssl_ciphers_, other.openssl_ciphers_);
    swap(openssl_sni_hostname_, other.openssl_sni_hostname_);
    swap(openssl_options_, other.openssl_options_);
    swap(io_service_, other.io_service_);
    swap(always_verify_peer_, other.always_verify_peer_);
    swap(timeout_, other.timeout_);
    swap(remove_chunk_markers_, other.remove_chunk_markers_);
  }

  /// Specify whether the client should cache resolved endpoints.
  ///
  /// Default: false.
  client_options& cache_resolved(bool v) {
    cache_resolved_ = v;
    return *this;
  }

  /// Specify whether the client should follow redirects.
  ///
  /// Default: false.
  /// \deprecated Not supported by asynchronous client implementation.
  client_options& follow_redirects(bool v) {
    follow_redirects_ = v;
    return *this;
  }

  /// Set the filename of the certificate to load for the SSL connection for
  /// verification.
  client_options& openssl_certificate(string_type const& v) {
    openssl_certificate_ = boost::make_optional(v);
    return *this;
  }

  /// Set the directory for which the certificate authority files are located.
  client_options& openssl_verify_path(string_type const& v) {
    openssl_verify_path_ = boost::make_optional(v);
    return *this;
  }

  /// Set the filename of the certificate to use for client-side SSL session
  /// establishment.
  client_options& openssl_certificate_file(string_type const& v) {
    openssl_certificate_file_ = boost::make_optional(v);
    return *this;
  }

  /// Set the filename of the private key to use for client-side SSL session
  /// establishment.
  client_options& openssl_private_key_file(string_type const& v) {
    openssl_private_key_file_ = boost::make_optional(v);
    return *this;
  }

  /// Set the ciphers to support for SSL negotiation.
  client_options& openssl_ciphers(string_type const& v) {
    openssl_ciphers_ = boost::make_optional(v);
    return *this;
  }

  /// Set the hostname for SSL SNI hostname support.
  client_options& openssl_sni_hostname(string_type const& v) {
    openssl_sni_hostname_ = boost::make_optional(v);
    return *this;
  }

  /// Set the raw OpenSSL options to use for HTTPS requests.
  client_options& openssl_options(long o) {
    openssl_options_ = o;
    return *this;
  }

  /// Provide an `boost::asio::io_service` hosted in a shared pointer.
  client_options& io_service(std::shared_ptr<boost::asio::io_service> v) {
    io_service_ = v;
    return *this;
  }

  /// Set whether we always verify the peer on the other side of the HTTPS
  /// connection.
  ///
  /// Default: true.
  client_options& always_verify_peer(bool v) {
    always_verify_peer_ = v;
    return *this;
  }

  /// Set an overall timeout for HTTP requests.
  client_options& timeout(int v) {
    timeout_ = v;
    return *this;
  }

  /// Set whether we process chunked-encoded streams.
  client_options& remove_chunk_markers(bool v) {
    remove_chunk_markers_ = v;
    return *this;
  }

  bool cache_resolved() const { return cache_resolved_; }

  bool follow_redirects() const { return follow_redirects_; }

  boost::optional<string_type> openssl_certificate() const {
    return openssl_certificate_;
  }

  boost::optional<string_type> openssl_verify_path() const {
    return openssl_verify_path_;
  }

  boost::optional<string_type> openssl_certificate_file() const {
    return openssl_certificate_file_;
  }

  boost::optional<string_type> openssl_private_key_file() const {
    return openssl_private_key_file_;
  }

  boost::optional<string_type> openssl_ciphers() const {
    return openssl_ciphers_;
  }

  boost::optional<string_type> openssl_sni_hostname() const {
    return openssl_sni_hostname_;
  }

  long openssl_options() const { return openssl_options_; }

  std::shared_ptr<boost::asio::io_service> io_service() const { return io_service_; }

  bool always_verify_peer() const { return always_verify_peer_; }

  int timeout() const { return timeout_; }

  bool remove_chunk_markers() const { return remove_chunk_markers_; }

 private:
  bool cache_resolved_;
  bool follow_redirects_;
  boost::optional<string_type> openssl_certificate_;
  boost::optional<string_type> openssl_verify_path_;
  boost::optional<string_type> openssl_certificate_file_;
  boost::optional<string_type> openssl_private_key_file_;
  boost::optional<string_type> openssl_ciphers_;
  boost::optional<string_type> openssl_sni_hostname_;
  long openssl_options_;
  std::shared_ptr<boost::asio::io_service> io_service_;
  bool always_verify_peer_;
  int timeout_;
  bool remove_chunk_markers_;
};

template <class Tag>
inline void swap(client_options<Tag>& a, client_options<Tag>& b) {
  a.swap(b);
}

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_OPTIONS_HPP_20130128
