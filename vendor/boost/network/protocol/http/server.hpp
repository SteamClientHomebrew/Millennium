// Copyright 2009 (c) Tarro, Inc.
// Copyright 2009 (c) Dean Michael Berris <mikhailberis@gmail.com>
// Copyright 2010 (c) Glyn Matthews
// Copyright Google, Inc. 2015
// Copyright 2003-2008 (c) Chris Kholhoff
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_HTTP_SERVER_HPP_
#define BOOST_NETWORK_HTTP_SERVER_HPP_

#include <boost/network/protocol/http/response.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/server/async_server.hpp>

namespace boost {
namespace network {
namespace http {

/**
 * The main HTTP Server template implementing an asynchronous HTTP service.
 *
 * Usage Example:
 * \code{.cpp}
 *     handler_type handler;
 *     http_server::options options(handler);
 *     options.thread_pool(
 *         std::make_shared<boost::network::utils::thread_pool>());
 *     http_server server(options.address("localhost").port("8000"));
 * \endcode
 *
 */
template <class Handler>
struct server : async_server_base<tags::http_server, Handler> {
  /// A convenience typedef for the base of this type, implementing most of
  /// the internal details.
  typedef async_server_base<tags::http_server, Handler> server_base;

  /// The options supported by the server.
  typedef server_options<tags::http_server, Handler> options;

  using server_base::server_base;
};

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_HTTP_SERVER_HPP_
