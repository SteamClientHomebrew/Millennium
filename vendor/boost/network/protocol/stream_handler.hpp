#ifndef NETLIB_IO_STREAM_HANDLER_HPP
#define NETLIB_IO_STREAM_HANDLER_HPP

// Copyright 2014 Jelle Van den Driessche.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif  // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <memory>
#include <boost/asio/async_result.hpp>
#include <boost/asio/basic_socket.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/handler_type_requirements.hpp>
#include <boost/asio/detail/push_options.hpp>
#include <boost/asio/detail/throw_error.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>
#include <cstddef>

#ifdef BOOST_NETWORK_ENABLE_HTTPS
#include <boost/asio/ssl.hpp>
#endif

namespace boost {
namespace network {

typedef boost::asio::ip::tcp::socket tcp_socket;

#ifndef BOOST_NETWORK_ENABLE_HTTPS
typedef tcp_socket stream_handler;
typedef void ssl_context;
#else

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
typedef boost::asio::ssl::context ssl_context;

struct stream_handler {
 public:
  typedef tcp_socket::executor_type executor_type;
  stream_handler(std::shared_ptr<tcp_socket> socket)
      : tcp_sock_(std::move(socket)), ssl_enabled(false) {}

  ~stream_handler() = default;

  stream_handler(std::shared_ptr<ssl_socket> socket)
      : ssl_sock_(std::move(socket)), ssl_enabled(true) {}

  stream_handler(boost::asio::io_service& io,
                 std::shared_ptr<ssl_context> ctx =
                     std::shared_ptr<ssl_context>()) {
    tcp_sock_ = std::make_shared<tcp_socket>(io.get_executor());
    ssl_enabled = false;
    if (ctx) {
      /// SSL is enabled
      ssl_sock_ =
          std::make_shared<ssl_socket>(io.get_executor(), boost::ref(*ctx));
      ssl_enabled = true;
    }
  }

  template <typename ConstBufferSequence, typename WriteHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
                                void(boost::system::error_code, std::size_t))
      async_write_some(const ConstBufferSequence& buffers,
                       BOOST_ASIO_MOVE_ARG(WriteHandler) handler) {
    try {
      if (ssl_enabled) {
        ssl_sock_->async_write_some(buffers, handler);
      } else {
        tcp_sock_->async_write_some(buffers, handler);
      }
    }
    catch (const boost::system::system_error&) {
      // print system error
    }
  }

  template <typename MutableBufferSequence, typename ReadHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
                                void(boost::system::error_code, std::size_t))
      async_read_some(const MutableBufferSequence& buffers,
                      BOOST_ASIO_MOVE_ARG(ReadHandler) handler) {
    try {
      if (ssl_enabled) {
        ssl_sock_->async_read_some(buffers, handler);
      } else {
        tcp_sock_->async_read_some(buffers, handler);
      }
    }
    catch (const boost::system::system_error& e) {
      // print system error
    }
  }

  void close(boost::system::error_code& e) {
    if (ssl_enabled) {
      ssl_sock_->next_layer().close();
    } else {
      tcp_sock_->close();
    }
  }

  tcp_socket::endpoint_type remote_endpoint(boost::system::error_code& ec) const {
    if (ssl_enabled) {
      return ssl_sock_->next_layer().remote_endpoint(ec);
    } else {
      return tcp_sock_->remote_endpoint(ec);
    }
  }

  tcp_socket::endpoint_type remote_endpoint() const {
    boost::system::error_code ec;
    tcp_socket::endpoint_type r = remote_endpoint(ec);
    if (ec) {
      boost::asio::detail::throw_error(ec, "remote_endpoint");
    }
    return r;
  }

  void shutdown(boost::asio::socket_base::shutdown_type st,
                boost::system::error_code& e) {
    try {
      if (ssl_enabled) {
        ssl_sock_->shutdown(e);
      } else {
        tcp_sock_->shutdown(boost::asio::ip::tcp::socket::shutdown_send, e);
      }
    }
    catch (const boost::system::system_error&) {

    }
  }

  ssl_socket::next_layer_type& next_layer() const {
    if (ssl_enabled) {
      return ssl_sock_->next_layer();
    } else {
      return *tcp_sock_;
    }
  }

  ssl_socket::lowest_layer_type& lowest_layer() const {
    if (ssl_enabled) {
      return ssl_sock_->lowest_layer();
    } else {
      return tcp_sock_->lowest_layer();
    }
  }

  template <typename HandshakeHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(HandshakeHandler,
                                void(boost::system::error_code))
      async_handshake(ssl_socket::handshake_type type,
                      BOOST_ASIO_MOVE_ARG(HandshakeHandler) handler) {
    try {
      if (ssl_enabled) {
        return ssl_sock_->async_handshake(type, handler);
      } else {
        // NOOP
      }
    }
    catch (const boost::system::system_error& e) {

    }
  }
  std::shared_ptr<tcp_socket> get_tcp_socket() { return tcp_sock_; }
  std::shared_ptr<ssl_socket> get_ssl_socket() { return ssl_sock_; }

  bool is_ssl_enabled() { return ssl_enabled; }

 private:
  std::shared_ptr<tcp_socket> tcp_sock_;
  std::shared_ptr<ssl_socket> ssl_sock_;
  bool ssl_enabled;
};
#endif
}
}

#endif
