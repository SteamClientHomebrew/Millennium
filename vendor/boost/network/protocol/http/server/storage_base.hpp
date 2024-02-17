#ifndef BOOST_NETWORK_PROTOCOL_HTTP_SERVER_STORAGE_BASE_HPP_20101210
#define BOOST_NETWORK_PROTOCOL_HTTP_SERVER_STORAGE_BASE_HPP_20101210

// Copyright 2010 Dean Michael Berris.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <boost/network/protocol/http/server/options.hpp>

namespace boost {
namespace network {
namespace http {

struct server_storage_base {
  struct no_io_service {};
  struct has_io_service {};

 protected:
  template <class Tag, class Handler>
  explicit server_storage_base(server_options<Tag, Handler> const& options)
      : self_service_(options.io_service()
                          ? options.io_service()
                          : std::make_shared<boost::asio::io_service>()),
        service_(*self_service_) {}

  std::shared_ptr<boost::asio::io_service> self_service_;
  boost::asio::io_service& service_;
};

} /* http */

} /* network */

} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_SERVER_STORAGE_BASE_HPP_20101210 */
