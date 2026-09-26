#include <catch2/catch_test_macros.hpp>
#include "mep/sdk_ready_bus.h"

TEST_CASE("SDK readiness replays the last event by default", "[sdk][ready]")
{
    auto& bus = mep::sdk_ready_bus::instance();
    bus.notify({ "cached-sdk", "runtime" });
    std::vector<std::string> received;
    const int listener = bus.add_listener([&](const auto& event)
    {
        received.push_back(event.sdk_version);
    });
    bus.notify({ "new-sdk", "runtime" });
    bus.remove_listener(listener);
    bus.notify({ "after-unsubscribe", "runtime" });

    REQUIRE(received == std::vector<std::string>{ "cached-sdk", "new-sdk" });
}

TEST_CASE("SDK readiness without replay delivers only new events", "[sdk][ready]")
{
    auto& bus = mep::sdk_ready_bus::instance();
    bus.notify({ "cached-sdk", "runtime" });
    std::vector<std::string> received;
    const int listener = bus.add_listener([&](const auto& event)
    {
        received.push_back(event.sdk_version);
    }, false);
    const bool replayed = !received.empty();
    bus.notify({ "new-sdk", "runtime" });
    bus.remove_listener(listener);
    bus.notify({ "after-unsubscribe", "runtime" });

    REQUIRE_FALSE(replayed);
    REQUIRE(received == std::vector<std::string>{ "new-sdk" });
    REQUIRE(bus.get_last()->sdk_version == "after-unsubscribe");
}
