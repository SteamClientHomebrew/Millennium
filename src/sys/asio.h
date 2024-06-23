#pragma once
#include <asio.hpp>
#include <asio/ip/tcp.hpp>

namespace Asio {
    static asio::ip::port_type GetRandomOpenPort() 
    {
        asio::io_context io_context;
        asio::ip::tcp::acceptor acceptor(io_context);
        asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), 0);

        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();
        return acceptor.local_endpoint().port();
    }
}