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

struct fixture
{
    std::mutex mutex;
    std::vector<json> sent;
    std::atomic<int> errors{0};
    cdp_client client;

    fixture() : client([this](const std::string& payload)
    {
        std::lock_guard<std::mutex> lock(mutex);
        sent.push_back(json::parse(payload));
        return true;
    })
    {
        client.set_error_handler([this](const std::string&, const std::exception&) { ++errors; });
    }

    int last_id()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return sent.back().at("id").get<int>();
    }

    size_t send_count()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return sent.size();
    }

    void event(const std::string& method, const json& params, const std::string& session = "")
    {
        json message = {{"method", method}, {"params", params}};
        if (!session.empty()) message["sessionId"] = session;
        client.handle_message(message.dump());
    }

    void result(int id, const json& value = json::object())
    {
        client.handle_message(json{{"id", id}, {"result", value}}.dump());
    }

    void sync()
    {
        auto response = client.send_host("Browser.getVersion");
        result(last_id());
        require(response.wait_for(1s) == std::future_status::ready, "Core dispatch stalled");
        response.get();
    }

    void attach(const std::string& target, const std::string& session, const std::string& parent = "")
    {
        auto response = client.send_host("Target.attachToTarget", {{"targetId", target}, {"flatten", true}}, parent.empty() ? std::nullopt : std::optional<std::string>(parent));
        result(last_id(), {{"sessionId", session}});
        require(response.wait_for(1s) == std::future_status::ready, "Attach response stalled");
        require(response.get().at("sessionId") == session, "Attach response changed");
    }
};

static void require_cancelled(std::future<json>& response, const std::string& target, const std::string& session)
{
    require(response.wait_for(1s) == std::future_status::ready, "Owner closure left request unresolved");
    try {
        response.get();
    } catch (const cdp_cancelled& error) {
        require(error.target_id == target && error.session_id == session, "Cancellation lost owner context");
        return;
    }
    throw std::runtime_error("Owner closure was not typed cancellation");
}

static void command_responses()
{
    fixture f;
    const auto respond = [&](json message, const std::string& session = "ordinary")
    {
        auto response = f.client.send_host("Runtime.evaluate", json::object(), session);
        message["id"] = f.last_id();
        f.client.handle_message(message.dump());
        require(response.wait_for(1s) == std::future_status::ready, "CDP response left unresolved");
        return response;
    };
    const auto require_error = [&](const json& message, const std::string& expected)
    {
        auto response = respond(message);
        try {
            response.get();
        } catch (const cdp_cancelled&) {
            throw std::runtime_error("Ordinary failure cancelled its owner");
        } catch (const std::runtime_error& error) {
            require(error.what() == expected, "CDP error message changed");
            return;
        }
        throw std::runtime_error("CDP error unexpectedly succeeded");
    };
    require_error({{"error", {{"code", -32602}, {"message", "Execution context was destroyed."}}}}, "Execution context was destroyed.");
    require_error({{"error", {{"message", "No target with given id found."}}}}, "No target with given id found.");
    require_error({{"error", {{"code", -32602}}}}, "CDP Error");
    require_error({{"error", {{"message", 42}}}}, "CDP Error");
    require_error({{"error", {{"code", 4294967296LL}, {"message", "oversized code"}}}}, "oversized code");
    require_error({{"error", 42}}, "CDP Error");
    require_error(json::object(), "Invalid CDP response");
    auto structured = respond({{"error", {{"code", -32000}, {"message", "unrelated"}}}});
    try {
        structured.get();
        throw std::logic_error("Protocol failure succeeded");
    } catch (const cdp_protocol_error& error) {
        require(error.code == -32000 && error.method == "Runtime.evaluate", "Protocol details lost");
    }
    const json expected = {{"result", {{"type", "number"}, {"value", 42}}}};
    auto successful = respond({{"result", expected}});
    require(successful.get() == expected, "Successful response changed");
    auto pending = f.client.send_host("Runtime.evaluate", {}, "ordinary");
    const int id = f.last_id();
    f.result(id + 1000, {{"wrong", true}});
    f.client.handle_message(json{{"id", 4294967296LL + id}, {"result", {{"wrong", true}}}}.dump());
    f.sync();
    require(pending.wait_for(0ms) != std::future_status::ready, "Unknown request ID completed pending work");
    f.result(id, expected);
    f.result(id, {{"duplicate", true}});
    require(pending.wait_for(1s) == std::future_status::ready && pending.get() == expected, "ID matching failed");
    auto unowned = f.client.send_host("Browser.getVersion");
    f.client.handle_message(json{{"id", f.last_id()}, {"error", {{"code", -32602}, {"message", "No target with given id found"}}}}.dump());
    require(unowned.wait_for(1s) == std::future_status::ready, "Unowned error stalled");
    try {
        unowned.get();
        throw std::logic_error("Unowned error succeeded");
    } catch (const cdp_cancelled&) {
        throw std::runtime_error("Unowned error cancelled browser");
    } catch (const cdp_protocol_error&) {
    }
    require(f.client.is_session_active("ordinary"), "Navigation failure closed session");
    std::promise<void> reentered;
    auto reentry = reentered.get_future();
    f.client.set_error_handler([&](const std::string&, const std::exception&)
    {
        f.client.set_error_handler({});
        f.client.send_host("Browser.getVersion");
        reentered.set_value();
    });
    f.client.handle_message("not JSON");
    require(reentry.wait_for(1s) == std::future_status::ready, "Error callback could not reenter client");
    f.client.shutdown();
}

static void lifecycle_closure()
{
    fixture f;
    f.attach("dead", "dead-1");
    f.attach("dead", "dead-2");
    f.attach("nested", "nested-1", "dead-1");
    f.attach("live", "live-1");
    f.client.set_shared_js_session_id("dead-1");
    auto first = f.client.send("Runtime.evaluate");
    const int first_id = f.last_id();
    auto second = f.client.send_host("Page.reload", {}, "dead-2");
    auto nested = f.client.send_host("Runtime.evaluate", {}, "nested-1");
    auto attaching = f.client.send_host("Target.attachToTarget", {{"targetId", "dead"}, {"flatten", true}});
    const int attaching_id = f.last_id();
    auto live = f.client.send_host("Runtime.evaluate", {}, "live-1");
    const int live_id = f.last_id();
    auto browser = f.client.send_host("Browser.getVersion");
    const int browser_id = f.last_id();
    f.event("Target.targetDestroyed", {{"targetId", "dead"}});
    require_cancelled(first, "dead", "dead-1");
    require_cancelled(second, "dead", "dead-2");
    require_cancelled(nested, "nested", "nested-1");
    require_cancelled(attaching, "dead", "");
    require(f.client.get_shared_js_session_id().empty(), "Dead shared session retained");
    const auto count = f.send_count();
    auto rejected = f.client.send_host("Page.reload", {}, "dead-1");
    require_cancelled(rejected, "dead", "dead-1");
    auto rejected_attach = f.client.send_host("Target.attachToTarget", {{"targetId", "dead"}});
    require_cancelled(rejected_attach, "dead", "");
    auto shared = f.client.send("Runtime.evaluate");
    require_cancelled(shared, "", "");
    require(f.send_count() == count, "Known-dead work reached transport");
    f.result(first_id);
    f.result(attaching_id, {{"sessionId", "late"}});
    f.event("Target.attachedToTarget", {{"sessionId", "late"}, {"targetInfo", {{"targetId", "dead"}}}});
    f.sync();
    require(!f.client.is_session_active("late"), "Late attach revived target");
    try {
        f.client.set_shared_js_session_id("late");
        throw std::logic_error("Stale shared session accepted");
    } catch (const cdp_cancelled&) {
    }
    require(live.wait_for(0ms) != std::future_status::ready && browser.wait_for(0ms) != std::future_status::ready, "Closure cancelled healthy work");
    f.result(live_id, {{"live", true}});
    f.result(browser_id, {{"browser", true}});
    require(live.wait_for(1s) == std::future_status::ready && live.get().at("live") == true, "Healthy target stopped");
    require(browser.wait_for(1s) == std::future_status::ready && browser.get().at("browser") == true, "Browser stopped");
    auto surviving = f.client.send_host("Runtime.evaluate", {}, "live-1");
    const int surviving_id = f.last_id();
    f.event("Target.targetCrashed", {{"targetId", "live"}});
    f.sync();
    require(surviving.wait_for(0ms) != std::future_status::ready, "Renderer crash cancelled a surviving session");
    require(f.client.is_target_active("live") && f.client.is_session_active("live-1"), "Renderer crash permanently closed its owner");
    f.result(surviving_id, {{"survived", true}});
    require(surviving.wait_for(1s) == std::future_status::ready && surviving.get().at("survived") == true, "Surviving session stopped resolving requests");
    std::promise<void> binding_seen;
    auto binding = binding_seen.get_future();
    f.client.on("Runtime.bindingCalled", [&](const json&) { binding_seen.set_value(); });
    f.event("Inspector.targetReloadedAfterCrash", {}, "live-1");
    f.event("Runtime.bindingCalled", {{"name", "recovered"}}, "live-1");
    require(binding.wait_for(1s) == std::future_status::ready, "Recovered session dropped its binding callback");
    const auto before_recovered = f.send_count();
    auto recovered = f.client.send_host("Runtime.evaluate", {}, "live-1");
    require(f.send_count() == before_recovered + 1, "Recovered session did not reach transport");
    f.result(f.last_id(), {{"recovered", true}});
    require(recovered.wait_for(1s) == std::future_status::ready && recovered.get().at("recovered") == true, "Recovered request failed");
    f.attach("live", "live-recovered");
    f.client.shutdown();
}

static void detached_sessions()
{
    fixture f;
    f.attach("parent-target", "parent");
    f.event("Target.attachedToTarget", {{"sessionId", "child"}, {"targetInfo", {{"targetId", "child-target"}}}}, "parent");
    f.event("Target.attachedToTarget", {{"sessionId", "grandchild"}, {"targetInfo", {{"targetId", "grand-target"}}}}, "child");
    f.sync();
    f.attach("child-target", "sibling", "parent");
    std::promise<json> observed;
    auto event_params = observed.get_future();
    f.client.on("Target.detachedFromTarget", [&](const json& params) { observed.set_value(params); });
    auto child = f.client.send_host("Runtime.evaluate", {}, "child");
    auto grandchild = f.client.send_host("Runtime.evaluate", {}, "grandchild");
    auto parent = f.client.send_host("Runtime.evaluate", {}, "parent");
    const int parent_id = f.last_id();
    auto sibling = f.client.send_host("Runtime.evaluate", {}, "sibling");
    const int sibling_id = f.last_id();
    f.event("Target.detachedFromTarget", {{"sessionId", "child"}, {"targetId", "child-target"}}, "parent");
    require_cancelled(child, "child-target", "child");
    require_cancelled(grandchild, "grand-target", "grandchild");
    require(event_params.wait_for(1s) == std::future_status::ready && event_params.get().at("sessionId") == "child", "Envelope overwrote detached child identity");
    require(f.client.is_target_active("child-target") && f.client.is_session_active("parent") && f.client.is_session_active("sibling"), "Detach killed parent, sibling or target");
    f.result(parent_id);
    f.result(sibling_id);
    require(parent.wait_for(1s) == std::future_status::ready, "Parent stalled"); parent.get();
    require(sibling.wait_for(1s) == std::future_status::ready, "Sibling stalled"); sibling.get();
    f.attach("child-target", "replacement", "parent");
    auto attaching = f.client.send_host("Target.attachToTarget", {{"targetId", "child-target"}}, "parent");
    const int attach_id = f.last_id();
    f.client.off_all("Target.detachedFromTarget");
    f.event("Target.detachedFromTarget", {{"sessionId", "early-detached"}}, "parent");
    f.result(attach_id, {{"sessionId", "early-detached"}});
    require_cancelled(attaching, "child-target", "early-detached");
    require(f.client.is_session_active("parent"), "Stale attach killed parent");
    f.client.off_all("Target.detachedFromTarget");
}

static void error_before_event()
{
    fixture f;
    auto attach = f.client.send_host("Target.attachToTarget", {{"targetId", "gone"}, {"flatten", true}});
    const int id = f.last_id();
    auto another = f.client.send_host("Target.attachToTarget", {{"targetId", "gone"}, {"flatten", true}});
    f.client.handle_message(json{{"id", id}, {"error", {{"code", -32602}, {"message", "No target with given id found"}}}}.dump());
    require(attach.wait_for(1s) == std::future_status::ready, "Attach error stalled");
    try {
        attach.get();
        throw std::logic_error("Missing target succeeded");
    } catch (const cdp_cancelled& error) {
        require(error.target_id == "gone" && error.session_id.empty() && error.code == -32602 && error.method == "Target.attachToTarget", "Attach cancellation details lost");
    }
    require_cancelled(another, "gone", "");
    f.event("Target.targetDestroyed", {{"targetId", "gone"}});
    f.attach("healthy", "parent");
    for (const std::string text : {"No session with given id", "Session with given id not found."}) {
        const auto session = text == "No session with given id" ? "one" : "two";
        f.attach("same-target", session, "parent");
        auto request = f.client.send_host("Target.detachFromTarget", {{"sessionId", session}}, "parent");
        f.client.handle_message(json{{"id", f.last_id()}, {"error", {{"code", -32001}, {"message", text}}}}.dump());
        require_cancelled(request, "same-target", session);
        require(f.client.is_session_active("parent") && f.client.is_target_active("same-target"), "Session failure invalidated parent or target");
    }
    f.attach("session-target", "session-gone");
    f.attach("session-target", "session-live");
    auto work = f.client.send_host("Runtime.evaluate", {}, "session-gone");
    f.client.handle_message(json{{"id", f.last_id()}, {"error", {{"code", -32602}, {"message", "No target with given id found"}}}}.dump());
    require_cancelled(work, "session-target", "session-gone");
    require(f.client.is_session_active("session-live") && f.client.is_target_active("session-target"), "Session-scoped target error closed siblings");
    f.sync();
}

static void blocked_callbacks()
{
    fixture f;
    f.attach("closing", "closing-session");
    std::array<std::promise<void>, 4> started;
    std::array<std::future<void>, 4> starts;
    for (size_t i = 0; i < starts.size(); ++i) starts[i] = started[i].get_future();
    std::atomic<int> next{0};
    std::atomic<int> cancelled{0};
    std::atomic<int> stale{0};
    f.client.on("Test.block", [&](const json&)
    {
        const int i = next.fetch_add(1);
        auto request = f.client.send_host("Runtime.addBinding", {}, "closing-session");
        started[i].set_value();
        try {
            request.get();
        } catch (const cdp_cancelled&) {
            ++cancelled;
            return;
        }
        throw std::runtime_error("Blocked callback unexpectedly succeeded");
    });
    for (int i = 0; i < 4; ++i) f.event("Test.block", {});
    for (auto& start : starts) require(start.wait_for(1s) == std::future_status::ready, "Callback workers did not block");
    f.client.on("Runtime.executionContextCreated", [&](const json&) { ++stale; });
    f.event("Runtime.executionContextCreated", {{"context", {{"id", 7}}}}, "closing-session");
    f.event("Target.targetDestroyed", {{"targetId", "closing"}});
    f.sync();
    f.client.shutdown();
    require(cancelled == 4, "Lifecycle processing depended on a free callback worker");
    require(stale == 0, "Known-stale queued callback ran");
}

static void completion_races()
{
    for (int i = 0; i < 80; ++i) {
        fixture f;
        f.attach("race", "race-session");
        auto request = f.client.send_host("Page.reload", {}, "race-session", 0ms);
        const int id = f.last_id();
        std::barrier start(4);
        std::thread response([&] { start.arrive_and_wait(); f.result(id, {{"winner", true}}); f.result(id); });
        std::thread closure([&] { start.arrive_and_wait(); f.event("Target.targetDestroyed", {{"targetId", "race"}}); });
        std::thread shutdown([&] { start.arrive_and_wait(); f.client.shutdown(); });
        start.arrive_and_wait();
        response.join(); closure.join(); shutdown.join();
        require(f.errors == 0, "Response race caused an internal completion error");
        require(request.wait_for(0ms) == std::future_status::ready, "Race left request unresolved");
        try {
            require(request.get().at("winner") == true, "Race response changed");
        } catch (const cdp_cancelled&) {
        } catch (const std::runtime_error& error) {
            require(std::string(error.what()).find("shutdown") != std::string::npos || std::string(error.what()) == "Request timeout", "Unexpected race outcome");
        }
    }
    fixture f;
    f.attach("timeout", "timed-session");
    auto timeout = f.client.send_host("Runtime.evaluate", {}, "timed-session", 0ms);
    require(timeout.wait_for(1500ms) == std::future_status::ready, "Timeout did not settle request");
    try {
        timeout.get();
        throw std::logic_error("Timeout succeeded");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()) == "Request timeout", "Timeout behavior changed");
    }
    f.result(f.last_id());
    f.event("Target.targetDestroyed", {{"targetId", "timeout"}});
    f.sync();
    fixture collision;
    collision.attach("collision", "collision-session");
    std::vector<std::future<json>> requests;
    std::vector<int> ids;
    for (int i = 0; i < 100; ++i) {
        requests.push_back(collision.client.send_host("Runtime.evaluate", {}, "collision-session", 0ms));
        ids.push_back(collision.last_id());
    }
    std::this_thread::sleep_for(995ms);
    std::thread responder([&] { for (int id : ids) collision.result(id, {{"winner", true}}); });
    std::thread closer([&] { collision.event("Target.targetDestroyed", {{"targetId", "collision"}}); });
    std::thread stopper([&] { std::this_thread::sleep_for(5ms); collision.client.shutdown(); });
    responder.join(); closer.join(); stopper.join();
    require(collision.errors == 0, "Cleanup race caused an internal completion error");
    for (auto& request : requests) {
        require(request.wait_for(0ms) == std::future_status::ready, "Cleanup race left pending request");
        try { require(request.get().at("winner") == true, "Cleanup race changed result"); }
        catch (const cdp_cancelled&) {}
        catch (const std::runtime_error& error) {
            require(std::string(error.what()) == "Request timeout" || std::string(error.what()).find("shutdown") != std::string::npos, "Cleanup race outcome changed");
        }
    }
    fixture ordered;
    ordered.attach("ordered", "ordered-session");
    auto completed = ordered.client.send_host("Runtime.evaluate", {}, "ordered-session");
    ordered.result(ordered.last_id(), {{"done", true}});
    ordered.event("Target.targetDestroyed", {{"targetId", "ordered"}});
    ordered.sync();
    require(completed.wait_for(0ms) == std::future_status::ready && completed.get().at("done") == true, "Closure rewrote completed response");
}

static void queued_transport()
{
    std::promise<void> entered;
    auto entry = entered.get_future();
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<int> sends{0};
    cdp_client client([&](const std::string&)
    {
        if (++sends == 1) { entered.set_value(); released.wait(); }
        return true;
    });
    auto blocker = std::async(std::launch::async, [&] { return client.send_host("Browser.getVersion"); });
    require(entry.wait_for(1s) == std::future_status::ready, "Transport did not block");
    auto queued = std::async(std::launch::async, [&] { return client.send_host("Target.attachToTarget", {{"targetId", "queued"}}); });
    client.handle_message(R"({"method":"Target.targetDestroyed","params":{"targetId":"queued"}})");
    std::promise<void> closed;
    auto closure = closed.get_future();
    client.on("Test.closed", [&](const json&) { closed.set_value(); });
    client.handle_message(R"({"method":"Test.closed","params":{}})");
    require(closure.wait_for(1s) == std::future_status::ready, "Closure could not bypass transport");
    release.set_value();
    auto response = queued.get();
    require_cancelled(response, "queued", "");
    require(sends == 1, "Known-stale queued send reached transport");
    auto pending = blocker.get();
    client.shutdown();
    require_shutdown_exception(pending);
    cdp_client failed([](const std::string&) { return false; });
    auto failure = failed.send_host("Browser.getVersion");
    require(failure.wait_for(0ms) == std::future_status::ready, "Send failure left request pending");
    try { failure.get(); throw std::logic_error("Send failure succeeded"); }
    catch (const std::runtime_error& error) { require(std::string(error.what()) == "Send failed", "Send failure changed"); }
    cdp_client throwing([](const std::string&) -> bool { throw std::runtime_error("transport exception"); });
    auto thrown = throwing.send_host("Browser.getVersion");
    require(thrown.wait_for(0ms) == std::future_status::ready, "Throwing transport left request pending");
    try { thrown.get(); throw std::logic_error("Throwing transport succeeded"); }
    catch (const std::runtime_error& error) { require(std::string(error.what()) == "transport exception", "Transport exception changed"); }
    for (bool close_owner : {false, true}) {
        std::promise<int> sent;
        auto sent_id = sent.get_future();
        std::promise<void> finish_send;
        auto finish = finish_send.get_future().share();
        cdp_client racing([&](const std::string& payload)
        {
            sent.set_value(json::parse(payload).at("id").get<int>());
            finish.wait();
            return false;
        });
        std::promise<void> processed;
        auto processing = processed.get_future();
        racing.on("Test.processed", [&](const json&) { processed.set_value(); });
        auto sending = std::async(std::launch::async, [&] { return racing.send_host("Target.attachToTarget", {{"targetId", "send-race"}}); });
        require(sent_id.wait_for(1s) == std::future_status::ready, "Racing send did not enter transport");
        const int id = sent_id.get();
        if (close_owner) racing.handle_message(R"({"method":"Target.targetDestroyed","params":{"targetId":"send-race"}})");
        else racing.handle_message(json{{"id", id}, {"result", {{"sessionId", "send-race-session"}}}}.dump());
        racing.handle_message(R"({"method":"Test.processed","params":{}})");
        require(processing.wait_for(1s) == std::future_status::ready, "Send failure race blocked core dispatch");
        finish_send.set_value();
        auto outcome = sending.get();
        if (close_owner) require_cancelled(outcome, "send-race", "");
        else require(outcome.get().at("sessionId") == "send-race-session", "Send failure replaced a real response");
        racing.shutdown();
    }
}

int main(int argc, char** argv)
{
    std::promise<void> done;
    auto completion = done.get_future();
    // Bound even the old deadlock without signalling any external process.
    std::thread watchdog([&]
    {
        if (completion.wait_for(8s) != std::future_status::ready) {
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
        else if (mode == "responses") command_responses();
        else if (mode == "lifecycle") lifecycle_closure();
        else if (mode == "detach") detached_sessions();
        else if (mode == "error-race") error_before_event();
        else if (mode == "blocked") blocked_callbacks();
        else if (mode == "races") completion_races();
        else if (mode == "queued") queued_transport();
        else throw std::runtime_error("Unknown test mode");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        status = EXIT_FAILURE;
    }
    done.set_value();
    watchdog.join();
    return status;
}
