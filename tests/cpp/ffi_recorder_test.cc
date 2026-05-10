/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "mep/ffi_recorder.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <stdexcept>
#include <string>

using namespace mep;

namespace {
std::string unique_plugin(const std::string &tag) {
  static std::atomic<int> counter{0};
  return tag + "_" + std::to_string(++counter);
}

ffi_call_entry make_entry(std::string plugin,
                          std::string method = "test_method") {
  return ffi_call_entry{
      .plugin = std::move(plugin),
      .method = std::move(method),
      .direction = "fe_to_be",
      .args = "{}",
      .result = "{}",
      .duration_ms = 1.0,
      .timestamp = std::chrono::system_clock::now(),
      .caller = "",
  };
}
} // namespace

TEST_CASE(
    "ffi_recorder: records entries and retrieves them in chronological order",
    "[ffi_recorder]") {
  auto &rec = ffi_recorder::instance();
  const auto plugin = unique_plugin("record_order");

  rec.record(make_entry(plugin, "method_a"));
  rec.record(make_entry(plugin, "method_b"));
  rec.record(make_entry(plugin, "method_c"));

  const auto entries = rec.get_recent(plugin, 10);
  REQUIRE(entries.size() == 3);
  CHECK(entries[0].method == "method_a");
  CHECK(entries[1].method == "method_b");
  CHECK(entries[2].method == "method_c");
}

TEST_CASE("ffi_recorder: filters returned entries by plugin name",
          "[ffi_recorder]") {
  auto &rec = ffi_recorder::instance();
  const auto plugin_a = unique_plugin("filter_a");
  const auto plugin_b = unique_plugin("filter_b");

  rec.record(make_entry(plugin_a, "a1"));
  rec.record(make_entry(plugin_b, "b1"));
  rec.record(make_entry(plugin_a, "a2"));

  CHECK(rec.get_recent(plugin_a, 10).size() == 2);
  CHECK(rec.get_recent(plugin_b, 10).size() == 1);
  CHECK(rec.get_recent(plugin_a, 10)[0].method == "a1");
  CHECK(rec.get_recent(plugin_a, 10)[1].method == "a2");
}

TEST_CASE("ffi_recorder: caps returned entries at the requested max",
          "[ffi_recorder]") {
  auto &rec = ffi_recorder::instance();
  const auto plugin = unique_plugin("max_cap");

  for (int i = 0; i < 5; ++i) {
    rec.record(make_entry(plugin, "m" + std::to_string(i)));
  }

  CHECK(rec.get_recent(plugin, 3).size() == 3);
  CHECK(rec.get_recent(plugin, 100).size() == 5);
  CHECK(rec.get_recent(plugin, 0).size() == 0);
}

TEST_CASE("ffi_recorder: invokes listeners for matching plugin only",
          "[ffi_recorder]") {
  auto &rec = ffi_recorder::instance();
  const auto plugin = unique_plugin("listener_match");
  const auto other = unique_plugin("listener_other");

  int call_count = 0;
  const int id = rec.add_listener(
      plugin, [&call_count](const ffi_call_entry &) { ++call_count; });

  rec.record(make_entry(plugin));
  rec.record(make_entry(plugin));
  rec.record(make_entry(other));

  CHECK(call_count == 2);

  rec.remove_listener(id);
  rec.record(make_entry(plugin));
  CHECK(call_count == 2); // listener removed: no further increments
}

TEST_CASE("ffi_recorder: tolerates listener exceptions without crashing",
          "[ffi_recorder]") {
  auto &rec = ffi_recorder::instance();
  const auto plugin = unique_plugin("listener_throw");

  int after_count = 0;
  const int throwing = rec.add_listener(
      plugin, [](const ffi_call_entry &) { throw std::runtime_error("boom"); });
  const int after = rec.add_listener(
      plugin, [&after_count](const ffi_call_entry &) { ++after_count; });

  REQUIRE_NOTHROW(rec.record(make_entry(plugin)));
  CHECK(after_count <= 1);

  rec.remove_listener(throwing);
  rec.remove_listener(after);
}
