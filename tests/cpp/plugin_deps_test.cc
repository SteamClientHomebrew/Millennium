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

#include "millennium/plugin_deps.h"
#include "millennium/semver.h"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>
#include <vector>

using plugin_list = std::vector<std::pair<std::string, std::vector<std::string>>>;

static std::vector<std::string> ordered_names(const plugin_list& plugins, const std::vector<size_t>& order)
{
    std::vector<std::string> names;
    for (const auto index : order) {
        names.push_back(plugins[index].first);
    }
    return names;
}

TEST_CASE("plugin_deps: a list without dependencies keeps its original order", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "charlie", {} },
        { "alpha",   {} },
        { "bravo",   {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "charlie", "alpha", "bravo" });
    REQUIRE(cycle.empty());
}

TEST_CASE("plugin_deps: a chain is ordered dependency-first", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "c", { "b" } },
        { "b", { "a" } },
        { "a", {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "a", "b", "c" });
    REQUIRE(cycle.empty());
}

TEST_CASE("plugin_deps: a diamond keeps unrelated plugins in scan order", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "top",   { "left", "right" } },
        { "left",  { "base" } },
        { "right", { "base" } },
        { "base",  {} },
        { "other", {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "base", "left", "right", "top", "other" });
    REQUIRE(cycle.empty());
}

TEST_CASE("plugin_deps: version ranges in specs are ignored for ordering", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "dependent", { "dependency@>=1.2.0" } },
        { "dependency", {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "dependency", "dependent" });
    REQUIRE(cycle.empty());
}

TEST_CASE("plugin_deps: unknown dependency names do not affect the order", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "first",  { "not-installed" } },
        { "second", {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "first", "second" });
    REQUIRE(cycle.empty());
}

TEST_CASE("plugin_deps: cycle members are appended in scan order and reported", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "ouroboros", { "ouroboros" } },
        { "ping",      { "pong" } },
        { "pong",      { "ping" } },
        { "normal",    {} },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "normal", "ouroboros", "ping", "pong" });
    REQUIRE(cycle == std::vector<std::string>{ "ouroboros", "ping", "pong" });
}

TEST_CASE("plugin_deps: duplicate plugin names resolve to the first occurrence", "[plugin_deps]")
{
    const plugin_list plugins = {
        { "dependent", { "twin" } },
        { "twin",      {} },
        { "twin",      { "dependent" } },
    };

    std::vector<std::string> cycle;
    const auto order = plugin_deps::resolve_load_order(plugins, cycle);

    /** the edge points at the first "twin"; the second one orders independently */
    REQUIRE(ordered_names(plugins, order) == std::vector<std::string>{ "twin", "dependent", "twin" });
    REQUIRE(cycle.empty());
}

TEST_CASE("semver: satisfies handles every supported operator", "[plugin_deps]")
{
    CHECK(semver::satisfies("1.2.3", ">=1.2.0"));
    CHECK_FALSE(semver::satisfies("1.1.0", ">=1.2.0"));
    CHECK(semver::satisfies("1.2.0", "<=1.2.0"));
    CHECK_FALSE(semver::satisfies("1.2.1", "<=1.2.0"));
    CHECK(semver::satisfies("2.0.0", ">1.9.9"));
    CHECK_FALSE(semver::satisfies("1.9.9", ">1.9.9"));
    CHECK(semver::satisfies("0.9.0", "<1.0.0"));
    CHECK_FALSE(semver::satisfies("1.0.0", "<1.0.0"));
    CHECK(semver::satisfies("1.2.3", "=1.2.3"));
    CHECK_FALSE(semver::satisfies("1.2.4", "=1.2.3"));
}

TEST_CASE("semver: satisfies treats a bare version as exact", "[plugin_deps]")
{
    CHECK(semver::satisfies("1.2.3", "1.2.3"));
    CHECK_FALSE(semver::satisfies("1.2.4", "1.2.3"));
}

TEST_CASE("semver: satisfies tolerates a leading v on either side", "[plugin_deps]")
{
    CHECK(semver::satisfies("v1.2.3", ">=1.2.0"));
    CHECK(semver::satisfies("1.2.3", ">=v1.2.0"));
}

TEST_CASE("semver: satisfies never rejects unparseable input", "[plugin_deps]")
{
    CHECK(semver::satisfies("", ">=1.2.0"));
    CHECK(semver::satisfies("not-a-version", ">=1.2.0"));
    CHECK(semver::satisfies("1.2.3", "garbage"));
}
