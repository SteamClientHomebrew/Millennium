#ifndef BOOST_NETWORK_COMPAT_HPP__
#define BOOST_NETWORK_COMPAT_HPP__

// Copyright 2019 (C) Enrico Detoma
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// These macros are used to solve a compatibility problem with Boost >= 1.70
// where the deprecated method get_io_service() was removed.
#if BOOST_VERSION >= 107000
#define CPP_NETLIB_ASIO_GET_IO_SERVICE(s) ((boost::asio::io_context&)(s).get_executor().context())
#define CPP_NETLIB_ASIO_IO_SERVICE_CONTEXT	boost::asio::io_context
#else
#define CPP_NETLIB_ASIO_GET_IO_SERVICE(s) ((s).get_io_service())
#define CPP_NETLIB_ASIO_IO_SERVICE_CONTEXT	boost::asio::io_service
#endif

#endif  // BOOST_NETWORK_COMPAT_HPP__
