#include <catch2/catch_test_macros.hpp>
#include "millennium/cdp_api.h"
#include <future>

using namespace std::chrono_literals;

TEST_CASE("CDP shutdown resolves requests before joining callbacks", "[cdp][shutdown]")
{
    std::promise<void> entered;
    auto waiting = entered.get_future();
    std::atomic<bool> failed{ false };
    auto client = std::make_shared<cdp_client>([](const std::string&)
    {
        return true;
    });
    client->on("test.wait", [&](const json&)
    {
        auto request = client->send_host("test.pending", json::object(), std::nullopt, 3s);
        entered.set_value();
        try {
            (void)request.get();
        } catch (const std::exception&) {
            failed.store(true);
        }
    });
    client->handle_message(R"({"method":"test.wait","params":{}})");
    REQUIRE(waiting.wait_for(2s) == std::future_status::ready);
    auto shutdown = std::async(std::launch::async, [&]
    {
        client->shutdown();
    });
    REQUIRE(shutdown.wait_for(1s) == std::future_status::ready);
    shutdown.get();
    REQUIRE(failed.load());
}

TEST_CASE("CDP queued sends do not enter the transport after shutdown", "[cdp][shutdown]")
{
    std::promise<void> entered;
    auto started = entered.get_future();
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<int> calls{ 0 };
    auto client = std::make_shared<cdp_client>([&](const std::string&)
    {
        if (++calls == 1) {
            entered.set_value();
            released.wait_for(3s);
        }
        return true;
    });
    auto first = std::async(std::launch::async, [&]
    {
        return client->send_host("first");
    });
    REQUIRE(started.wait_for(1s) == std::future_status::ready);
    auto second = std::async(std::launch::async, [&]
    {
        return client->send_host("second");
    });
    REQUIRE(second.wait_for(100ms) == std::future_status::timeout);
    client->shutdown();
    release.set_value();
    auto first_result = first.get();
    auto second_result = second.get();
    REQUIRE_THROWS(first_result.get());
    REQUIRE_THROWS(second_result.get());
    REQUIRE(calls.load() == 1);
}
