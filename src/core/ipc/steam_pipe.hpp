#pragma once
#include <utils/cout/logger.hpp>
#include <shellapi.h>
#include <window/core/window.hpp>
#include <window/win_handler.hpp>

#include <utils/thread/thread_handler.hpp>

using tcp = boost::asio::ip::tcp;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace websocket = boost::beast::websocket;

/// <summary>
/// inter program communication between the js on the steam webpage back to c++ here
/// </summary>
class millennium_ipc_listener {
public:
    uint16_t get_ipc_port() {
        return acceptor_.local_endpoint().port();
    }

    millennium_ipc_listener(boost::asio::io_context& ioc) :
        acceptor_(ioc, tcp::endpoint(tcp::v4(), 7894)),
        socket_(ioc) {
        ipc_do_accept();
    }
private:
    void ipc_do_accept() {
        //start listening for a new message
        acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
            if (!ec) {
                std::make_shared<millennium_ipc_session>(std::move(socket_))->ipc_initialize();
            }
            ipc_do_accept();
        });
    }

    class millennium_ipc_session : public std::enable_shared_from_this<millennium_ipc_session> {
    public:
        millennium_ipc_session(tcp::socket socket) : ws_(std::move(socket)) {}

        void ipc_initialize() {
            //initial socket startup
            ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
                if (!ec) {
                    self->read_request_payload();
                }
            });
        }

        void read_request_payload() 
        {
            //async read from the socket and wait for a message
            ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code read_error, std::size_t bytes_transferred) 
            {
                if (read_error) return;

                nlohmann::basic_json<> msg = nlohmann::json::parse(boost::beast::buffers_to_string(self->buffer_.data()));

                console.log(std::format("received a message, {}", msg.dump(4)));

                if (msg.value("open_millennium", false))
                {
                    console.log("opening Millennium");

                    if (!g_windowOpen)
                    {
                        threadContainer::getInstance().addThread(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)init_main_window, 0, 0, 0));
                    }
                    else {
                        Window::bringToFront();
                        PlaySoundA("SystemExclamation", NULL, SND_ALIAS);
                    }
                }

                self->buffer_.consume(bytes_transferred);
                self->read_request_payload();
            });
        }
    private:
        websocket::stream<tcp::socket> ws_;
        boost::beast::multi_buffer buffer_;
    };

    tcp::acceptor acceptor_;
    tcp::socket socket_;
};