#ifndef BOOST_NETWORK_PROTOCOL_HTTP_SERVER_OPTIONS_20130128
#define BOOST_NETWORK_PROTOCOL_HTTP_SERVER_OPTIONS_20130128

// Copyright 2013 Google, Inc.
// Copyright 2013 Dean Michael Berris <dberris@google.com>
// Copyright 2014 Jelle Van den Driessche
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/network/protocol/stream_handler.hpp>
#include <boost/network/traits/string.hpp>
#include <boost/network/utils/thread_pool.hpp>
#include <boost/optional.hpp>

namespace boost {
namespace network {
namespace http {

/**
 * The options supported by an HTTP Server's constructor.
 */
template <class Tag, class Handler>
struct server_options {
  typedef typename string<Tag>::type string_type;

  /// A single-argument constructor that takes a Handler, and sets all options
  /// to defaults.
  explicit server_options(Handler &handler)
      : io_service_(),
        handler_(handler),
        address_("localhost"),
        port_("80"),
        protocol_family_(undefined),
        reuse_address_(false),
        report_aborted_(false),
        non_blocking_io_(true),
        linger_(true),
        linger_timeout_(0),
        receive_buffer_size_(),
        send_buffer_size_(),
        receive_low_watermark_(),
        send_low_watermark_(),
        thread_pool_(),
        context_() {}

  /// Disabled default constructor for the options class.
  server_options() = delete;

  /// Copy constructor for the options class.
  server_options(const server_options &) = default;

  /// Copy assignment for the options class.
  server_options &operator=(const server_options &) = default;

  /// Move constructor for the options class.
  server_options(server_options &&) = default;

  /// Move assignment for the options class.
  server_options &operator=(server_options &&) = default;

  /// Destructor for the options class.
  ~server_options() = default;

  /// Sets the SSL context for the server. Default is nullptr.
  server_options &context(std::shared_ptr<ssl_context> v) {
    context_ = v;
    return *this;
  }

  /// Provides an Asio io_service for the server. Default is nullptr.
  server_options &io_service(std::shared_ptr<boost::asio::io_service> v) {
    io_service_ = v;
    return *this;
  }

  /// Sets the address to listen to for the server. Default is localhost.
  server_options &address(string_type v) {
    address_ = std::move(v);
    return *this;
  }

  /// Set the port to listen to for the server. Default is 80.
  server_options &port(string_type const &v) {
    port_ = v;
    return *this;
  }

  enum protocol_family_t { ipv4, ipv6, undefined };

  /// Set the protocol family for address resolving. Default is AF_UNSPEC.
  server_options &protocol_family(protocol_family_t v) {
    protocol_family_ = v;
    return *this;
  }

  /// Set whether to reuse the address (SO_REUSE_ADDR). Default is false.
  server_options &reuse_address(bool v) {
    reuse_address_ = v;
    return *this;
  }

  /// Set whether to report aborted connections. Default is false.
  server_options &report_aborted(bool v) {
    report_aborted_ = v;
    return *this;
  }

  /// Set whether to use non-blocking IO. Default is true.
  server_options &non_blocking_io(bool v) {
    non_blocking_io_ = v;
    return *this;
  }

  /// Set whether sockets linger (SO_LINGER). Default is true.
  server_options &linger(bool v) {
    linger_ = v;
    return *this;
  }

  /// Set the linger timeout. Default is 0.
  server_options &linger_timeout(size_t v) {
    linger_timeout_ = v;
    return *this;
  }

  /// Set the socket receive buffer size. Unset by default.
  server_options &receive_buffer_size(
      boost::asio::socket_base::receive_buffer_size v) {
    receive_buffer_size_ = v;
    return *this;
  }

  /// Set the send buffer size. Unset by default.
  server_options &send_buffer_size(boost::asio::socket_base::send_buffer_size v) {
    send_buffer_size_ = v;
    return *this;
  }

  /// Set the socket receive low watermark. Unset by default.
  server_options &receive_low_watermark(
      boost::asio::socket_base::receive_low_watermark v) {
    receive_low_watermark_ = v;
    return *this;
  }

  /// Set the socket send low watermark. Unset by default.
  server_options &send_low_watermark(boost::asio::socket_base::send_low_watermark v) {
    send_low_watermark_ = v;
    return *this;
  }

  /// Set the thread-pool to use. Default is nullptr.
  server_options &thread_pool(std::shared_ptr<utils::thread_pool> v) {
    thread_pool_ = v;
    return *this;
  }

  /// Returns the provided Asio io_service.
  std::shared_ptr<boost::asio::io_service> io_service() const { return io_service_; }

  /// Returns the address to listen on.
  string_type address() const { return address_; }

  /// Returns the port to listen on.
  string_type port() const { return port_; }

  /// Returns the protocol family used for address resolving.
  protocol_family_t protocol_family() const { return protocol_family_; }

  /// Returns a reference to the provided handler.
  Handler &handler() const { return handler_; }

  /// Returns whether to reuse the address.
  bool reuse_address() const { return reuse_address_; }

  /// Returns whether to report aborted connections.
  bool report_aborted() const { return report_aborted_; }

  /// Returns whether to perform non-blocking IO.
  bool non_blocking_io() const { return non_blocking_io_; }

  /// Returns whether to linger.
  bool linger() const { return linger_; }

  /// Returns the linger timeout.
  size_t linger_timeout() const { return linger_timeout_; }

  /// Returns the optional receive buffer size.
  boost::optional<boost::asio::socket_base::receive_buffer_size> receive_buffer_size()
      const {
    return receive_buffer_size_;
  }

  /// Returns the optional send buffer size.
  boost::optional<boost::asio::socket_base::send_buffer_size> send_buffer_size()
      const {
    return send_buffer_size_;
  }

  /// Returns the optional receive low watermark.
  boost::optional<boost::asio::socket_base::receive_low_watermark>
      receive_low_watermark() const {
    return receive_low_watermark_;
  }

  /// Returns the optional send low watermark.
  boost::optional<boost::asio::socket_base::send_low_watermark> send_low_watermark()
      const {
    return send_low_watermark_;
  }

  /// Returns a pointer to the provided thread pool.
  std::shared_ptr<utils::thread_pool> thread_pool() const {
    return thread_pool_;
  }

  /// Returns a pointer to the provided context.
  std::shared_ptr<ssl_context> context() const { return context_; }

  /// Swap implementation for the server options.
  void swap(server_options &other) {
    using std::swap;
    swap(io_service_, other.io_service_);
    swap(address_, other.address_);
    swap(port_, other.port_);
    swap(protocol_family_, other.protocol_family_);
    swap(reuse_address_, other.reuse_address_);
    swap(report_aborted_, other.report_aborted_);
    swap(non_blocking_io_, other.non_blocking_io_);
    swap(linger_, other.linger_);
    swap(linger_timeout_, other.linger_timeout_);
    swap(receive_buffer_size_, other.receive_buffer_size_);
    swap(send_buffer_size_, other.send_buffer_size_);
    swap(receive_low_watermark_, other.receive_low_watermark_);
    swap(send_low_watermark_, other.send_low_watermark_);
    swap(thread_pool_, other.thread_pool_);
    swap(context_, other.context_);
  }

 private:
  std::shared_ptr<boost::asio::io_service> io_service_;
  Handler &handler_;
  string_type address_;
  string_type port_;
  protocol_family_t protocol_family_;
  bool reuse_address_;
  bool report_aborted_;
  bool non_blocking_io_;
  bool linger_;
  size_t linger_timeout_;
  boost::optional<boost::asio::socket_base::receive_buffer_size> receive_buffer_size_;
  boost::optional<boost::asio::socket_base::send_buffer_size> send_buffer_size_;
  boost::optional<boost::asio::socket_base::receive_low_watermark>
      receive_low_watermark_;
  boost::optional<boost::asio::socket_base::send_low_watermark> send_low_watermark_;
  std::shared_ptr<utils::thread_pool> thread_pool_;
  std::shared_ptr<ssl_context> context_;
};

template <class Tag, class Handler>
inline void swap(server_options<Tag, Handler> &a,
                 server_options<Tag, Handler> &b) {
  a.swap(b);
}

} /* http */
} /* network */
} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_SERVER_OPTIONS_20130128 */
