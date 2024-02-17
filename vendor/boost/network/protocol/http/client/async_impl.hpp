#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623

// Copyright Dean Michael Berris 2010.
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <thread>
#include <memory>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/network/protocol/http/client/macros.hpp>
#include <boost/network/protocol/http/traits/connection_policy.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, unsigned version_major, unsigned version_minor>
struct basic_client_impl;

namespace impl {
template <class Tag, unsigned version_major, unsigned version_minor>
struct async_client
    : connection_policy<Tag, version_major, version_minor>::type {
  typedef typename connection_policy<Tag, version_major, version_minor>::type
      connection_base;
  typedef typename resolver<Tag>::type resolver_type;
  typedef typename string<Tag>::type string_type;

  typedef
      typename std::array<typename char_<Tag>::type,
                          BOOST_NETWORK_HTTP_CLIENT_CONNECTION_BUFFER_SIZE>::
          const_iterator const_iterator;
  typedef iterator_range<const_iterator> char_const_range;

  typedef std::function<void(char_const_range,
                             boost::system::error_code const&)>
      body_callback_function_type;

  typedef std::function<bool(string_type&)> body_generator_function_type;

  async_client(bool cache_resolved, bool follow_redirect,
               bool always_verify_peer, int timeout, bool remove_chunk_markers,
               std::shared_ptr<boost::asio::io_service> service,
               optional<string_type> certificate_filename,
               optional<string_type> verify_path,
               optional<string_type> certificate_file,
               optional<string_type> private_key_file,
               optional<string_type> ciphers,
               optional<string_type> sni_hostname, long ssl_options)
      : connection_base(cache_resolved, follow_redirect, timeout,
                        remove_chunk_markers),
        service_ptr(service.get() ? service
                                  : std::make_shared<boost::asio::io_service>()),
        service_(*service_ptr),
        resolver_(service_),
        sentinel_(new boost::asio::io_service::work(service_)),
        certificate_filename_(std::move(certificate_filename)),
        verify_path_(std::move(verify_path)),
        certificate_file_(std::move(certificate_file)),
        private_key_file_(std::move(private_key_file)),
        ciphers_(std::move(ciphers)),
        sni_hostname_(std::move(sni_hostname)),
        ssl_options_(ssl_options),
        always_verify_peer_(always_verify_peer) {
    connection_base::resolver_strand_.reset(
        new boost::asio::io_service::strand(service_));
    if (!service)
      lifetime_thread_.reset(new std::thread([this]() { service_.run(); }));
  }

  ~async_client() throw() = default;

  void wait_complete() {
    sentinel_.reset();
    if (lifetime_thread_.get()) {
      if (lifetime_thread_->joinable() && lifetime_thread_->get_id() != std::this_thread::get_id()) {
        lifetime_thread_->join();
      }
      lifetime_thread_.reset();
    }
  }

  basic_response<Tag> const request_skeleton(
      basic_request<Tag> const& request_, string_type const& method,
      bool get_body, body_callback_function_type callback,
      body_generator_function_type generator) {
    typename connection_base::connection_ptr connection_;
    connection_ = connection_base::get_connection(
        resolver_, request_, always_verify_peer_, certificate_filename_,
        verify_path_, certificate_file_, private_key_file_, ciphers_,
        sni_hostname_, ssl_options_);
    return connection_->send_request(method, request_, get_body, callback,
                                     generator);
  }

  std::shared_ptr<boost::asio::io_service> service_ptr;
  boost::asio::io_service& service_;
  resolver_type resolver_;
  std::shared_ptr<boost::asio::io_service::work> sentinel_;
  std::shared_ptr<std::thread> lifetime_thread_;
  optional<string_type> certificate_filename_;
  optional<string_type> verify_path_;
  optional<string_type> certificate_file_;
  optional<string_type> private_key_file_;
  optional<string_type> ciphers_;
  optional<string_type> sni_hostname_;
  long ssl_options_;
  bool always_verify_peer_;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623
