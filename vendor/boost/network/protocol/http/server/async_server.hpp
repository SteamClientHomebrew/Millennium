#ifndef BOOST_NETWORK_PROTOCOL_HTTP_SERVER_ASYNC_SERVER_HPP_20101025
#define BOOST_NETWORK_PROTOCOL_HTTP_SERVER_ASYNC_SERVER_HPP_20101025

// Copyright 2010 Dean Michael Berris.
// Copyright 2014 Jelle Van den Driessche.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <mutex>
#include <boost/network/detail/debug.hpp>
#include <boost/network/protocol/http/server/async_connection.hpp>
#include <boost/network/protocol/http/server/storage_base.hpp>
#include <boost/network/protocol/http/server/socket_options_base.hpp>
#include <boost/network/utils/thread_pool.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag, class Handler>
struct async_server_base : server_storage_base, socket_options_base {
  /// The request type for this server.
  typedef basic_request<Tag> request;
  typedef typename string<Tag>::type string_type;

  /// The header type for this server.
  typedef
      typename boost::network::http::response_header<Tag>::type response_header;

  /// The connection type for this server.
  typedef async_connection<Tag, Handler> connection;

  /// Defines the type for the connection pointer.
  typedef std::shared_ptr<connection> connection_ptr;

  /// Defines the type for the options.
  typedef server_options<Tag, Handler> options;

  /// Constructs and initializes the asynchronous server core.
  explicit async_server_base(options const &options)
      : server_storage_base(options),
        socket_options_base(options),
        handler(options.handler()),
        address_(options.address()),
        port_(options.port()),
        protocol_family(options.protocol_family()),
        thread_pool(options.thread_pool()
                        ? options.thread_pool()
                        : std::make_shared<utils::thread_pool>()),
        acceptor(server_storage_base::service_),
        stopping(false),
        new_connection(),
        listening_mutex_(),
        stopping_mutex_(),
        listening(false),
        ctx_(options.context()) {}

  /**
   * Listens to the correct port and runs the server's event loop. This can be
   * run on multiple threads, as in the example below:
   *
   * Example:
   *    handler_type handler;
   *    http_server::options options(handler);
   *    options.thread_pool(
   *        std::make_shared<boost::network::utils::thread_pool>());
   *    http_server server(options.address("localhost").port("8000"));
   *
   *    // Run in three threads including the current thread.
   *    std::thread t1([&server] { server.run() });
   *    std::thread t2([&server] { server.run() });
   *    server.run();
   *    t1.join();
   *    t2.join();
   */
  void run() {
    listen();
    service_.run();
  };

  /// Stops the HTTP server acceptor and waits for all pending request handlers
  /// to finish.
  void stop() {
    // stop accepting new requests and let all the existing
    // handlers finish.
    scoped_mutex_lock listening_lock(listening_mutex_);
    if (listening) {  // we dont bother stopping if we arent currently
                      // listening
      scoped_mutex_lock stopping_lock(stopping_mutex_);
      stopping = true;
      boost::system::error_code ignored;
      acceptor.close(ignored);
      listening = false;
      service_.post([this]() { this->handle_stop(); });
    }
  }

  /// Explicitly listens on the configured host and port. May be called
  /// multiple times but only takes effect once.
  void listen() {
    scoped_mutex_lock listening_lock(listening_mutex_);
    BOOST_NETWORK_MESSAGE("Listening on " << address_ << ':' << port_);
    if (!listening)
      start_listening();  // we only initialize our acceptor/sockets if we
                          // arent already listening
    if (!listening) {
      BOOST_NETWORK_MESSAGE("Error listening on " << address_ << ':' << port_);
      boost::throw_exception(
          std::runtime_error("Error listening on provided port."));
    }
  }

  /// Returns the server socket address, either IPv4 or IPv6 depending on
  /// options.protocol_family()
  const string_type& address() const { return address_; }

  /// Returns the server socket port
  const string_type& port() const { return port_; }

 private:
  typedef std::unique_lock<std::mutex> scoped_mutex_lock;

  Handler &handler;
  string_type address_, port_;
  typename options::protocol_family_t protocol_family;
  std::shared_ptr<utils::thread_pool> thread_pool;
  boost::asio::ip::tcp::acceptor acceptor;
  bool stopping;
  connection_ptr new_connection;
  std::mutex listening_mutex_;
  std::mutex stopping_mutex_;
  bool listening;
  std::shared_ptr<ssl_context> ctx_;

  void handle_stop() {
    scoped_mutex_lock stopping_lock(stopping_mutex_);
    if (stopping)
      service_.stop();  // a user may have started listening again before
                        // the stop command is reached
  }

  void handle_accept(boost::system::error_code const &ec) {
    {
      scoped_mutex_lock stopping_lock(stopping_mutex_);
      if (stopping)
        return;  // we dont want to add another handler instance, and we
                 // dont want to know about errors for a socket we dont
                 // need anymore
    }

    if (ec) {
      BOOST_NETWORK_MESSAGE("Error accepting connection, reason: " << ec);
    }

#ifdef BOOST_NETWORK_ENABLE_HTTPS
    socket_options_base::socket_options(new_connection->socket().next_layer());
#else
    socket_options_base::socket_options(new_connection->socket());
#endif

    new_connection->start();
    new_connection.reset(new connection(service_, handler, *thread_pool, ctx_));
    acceptor.async_accept(
#ifdef BOOST_NETWORK_ENABLE_HTTPS
        new_connection->socket().next_layer(),
#else
        new_connection->socket(),
#endif
        [this](boost::system::error_code const &ec) { this->handle_accept(ec); });
  }

  void start_listening() {
    using boost::asio::ip::tcp;
    boost::system::error_code error;
    // this allows repeated cycles of run -> stop -> run
    service_.reset();
    tcp::resolver resolver(service_);
    tcp::resolver::query query( [&]{
        switch(protocol_family) {
        case options::ipv4:
            return tcp::resolver::query(tcp::v4(), address_, port_);
        case options::ipv6:
            return tcp::resolver::query(tcp::v6(), address_, port_);
        default:
            return tcp::resolver::query(address_, port_);
        }}());
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, error);
    if (error) {
      BOOST_NETWORK_MESSAGE("Error resolving '" << address_ << ':' << port_);
      return;
    }
    tcp::endpoint endpoint = *endpoint_iterator;
    acceptor.open(endpoint.protocol(), error);
    if (error) {
      BOOST_NETWORK_MESSAGE("Error opening socket: " << address_ << ":"
                                                     << port_);
      return;
    }
    socket_options_base::acceptor_options(acceptor);
    acceptor.bind(endpoint, error);
    if (error) {
      BOOST_NETWORK_MESSAGE("Error binding socket: " << address_ << ":"
                                                     << port_);
      return;
    }
    address_ = acceptor.local_endpoint().address().to_string();
    port_ = std::to_string(acceptor.local_endpoint().port());
    acceptor.listen(boost::asio::socket_base::max_connections, error);
    if (error) {
      BOOST_NETWORK_MESSAGE("Error listening on socket: '"
                            << error << "' on " << address_ << ":" << port_);
      return;
    }
    new_connection.reset(new connection(service_, handler, *thread_pool, ctx_));
    acceptor.async_accept(
#ifdef BOOST_NETWORK_ENABLE_HTTPS
        new_connection->socket().next_layer(),
#else
        new_connection->socket(),
#endif
        [this](boost::system::error_code const &ec) { this->handle_accept(ec); });
    listening = true;
    scoped_mutex_lock stopping_lock(stopping_mutex_);
    stopping = false;  // if we were in the process of stopping, we revoke
                       // that command and continue listening
    BOOST_NETWORK_MESSAGE("Now listening on socket: '" << address_ << ":"
                                                       << port_ << "'");
  }
};

} /* http */

} /* network */

} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_SERVER_ASYNC_SERVER_HPP_20101025 */
