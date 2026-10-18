#include "millennium/cdp_api.h"
#include <array>
#include <barrier>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std::chrono_literals;

static void require(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void require_shutdown_exception(std::future<json>& response)
{
    require(response.wait_for(0ms) == std::future_status::ready, "CDP request left unresolved after shutdown");
    try {
        response.get();
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find("shutdown") != std::string::npos, "Unexpected CDP exception");
        return;
    }
    throw std::runtime_error("Shutdown request unexpectedly succeeded");
}

static void callback_waiting_for_response()
{
    cdp_client client([](const std::string&) { return true; });
    std::promise<void> request_sent;
    auto sent = request_sent.get_future();
    std::promise<void> callback_finished;
    auto finished = callback_finished.get_future();
    int token = client.on("Runtime.executionContextCreated", [&](const json&)
    {
        // Match ffi_binder: a callback synchronously waits for a CDP command
        // whose response can no longer arrive after the browser has exited.
        auto response = client.send_host("Runtime.addBinding", json::object(), std::nullopt, 1h);
        request_sent.set_value();
        try {
            response.get();
            throw std::logic_error("Unanswered request unexpectedly succeeded");
        } catch (const std::runtime_error& error) {
            require(std::string(error.what()).find("shutdown") != std::string::npos, "Unexpected callback exception");
        }
        // ffi_binder may attempt another binding after catching the first error.
        auto retry = client.send_host("Runtime.addBinding");
        require_shutdown_exception(retry);
        client.off_all("Runtime.executionContextCreated");
        callback_finished.set_value();
    });
    client.handle_message(R"({"method":"Runtime.executionContextCreated","params":{}})");
    require(sent.wait_for(1s) == std::future_status::ready, "Callback did not send its CDP request");
    client.shutdown();
    require(finished.wait_for(0ms) == std::future_status::ready, "Shutdown did not join the callback");
    client.off(token);
}

static void pending_requests()
{
    std::atomic<int> sends{0};
    cdp_client client([&](const std::string&) { ++sends; return true; });
    auto pending = client.send_host("Runtime.addBinding", json::object(), std::nullopt, 1h);
    client.shutdown();
    require_shutdown_exception(pending);
    auto rejected = client.send_host("Runtime.addBinding");
    require_shutdown_exception(rejected);
    require(sends == 1, "A request was sent after shutdown");
    client.shutdown();
}

static void concurrent_requests()
{
    cdp_client client([](const std::string&) { return true; });
    std::barrier start(5);
    std::array<std::thread, 4> senders;
    std::array<std::vector<std::future<json>>, 4> responses;
    for (size_t i = 0; i < senders.size(); ++i) {
        senders[i] = std::thread([&, i]
        {
            responses[i].push_back(client.send_host("Runtime.addBinding"));
            start.arrive_and_wait();
            for (int j = 0; j < 500; ++j) {
                responses[i].push_back(client.send_host("Runtime.addBinding"));
            }
        });
    }
    start.arrive_and_wait();
    client.shutdown();
    for (auto& sender : senders) sender.join();
    for (auto& batch : responses) {
        for (auto& response : batch) require_shutdown_exception(response);
    }
}

int main(int argc, char** argv)
{
    std::promise<void> done;
    auto completion = done.get_future();
    // Bound even the old deadlock without signalling any external process.
    std::thread watchdog([&]
    {
        if (completion.wait_for(3s) != std::future_status::ready) {
            std::cerr << "CDP shutdown did not complete: callback is waiting for an unresolved request\n";
            std::_Exit(EXIT_FAILURE);
        }
    });
    int status = EXIT_SUCCESS;
    try {
        require(argc == 2, "Expected a test mode");
        const std::string mode(argv[1]);
        if (mode == "callback") callback_waiting_for_response();
        else if (mode == "pending") pending_requests();
        else if (mode == "concurrent") concurrent_requests();
        else throw std::runtime_error("Unknown test mode");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = EXIT_FAILURE;
    }
    done.set_value();
    watchdog.join();
    return status;
}
