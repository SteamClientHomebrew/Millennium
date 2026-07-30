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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
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

#include <map>
#include <set>

std::vector<size_t> plugin_deps::resolve_load_order(const std::vector<std::pair<std::string, std::vector<std::string>>>& plugins, std::vector<std::string>& out_cycle)
{
    const size_t count = plugins.size();

    /** First occurrence wins when two plugins share a name. */
    std::map<std::string, size_t> index_by_name;
    for (size_t i = 0; i < count; ++i) {
        index_by_name.try_emplace(plugins[i].first, i);
    }

    /** Edge dep -> dependent; only between plugins present in the list. */
    std::vector<std::set<size_t>> dependents(count);
    std::vector<size_t> in_degree(count, 0);

    for (size_t i = 0; i < count; ++i) {
        for (const auto& spec : plugins[i].second) {
            const auto name = spec.substr(0, spec.find('@'));

            const auto it = index_by_name.find(name);
            if (it == index_by_name.end()) {
                continue;
            }

            if (dependents[it->second].insert(i).second) {
                in_degree[i]++;
            }
        }
    }

    /** Stable Kahn: always take the lowest original index that is ready, so
     *  plugins without ordering constraints keep their scan order. */
    std::set<size_t> ready;
    for (size_t i = 0; i < count; ++i) {
        if (in_degree[i] == 0) {
            ready.insert(i);
        }
    }

    std::vector<size_t> order;
    order.reserve(count);

    while (!ready.empty()) {
        const size_t current = *ready.begin();
        ready.erase(ready.begin());
        order.push_back(current);

        for (const size_t dependent : dependents[current]) {
            if (--in_degree[dependent] == 0) {
                ready.insert(dependent);
            }
        }
    }

    /** Whatever is left sits in a cycle; append it in the original order. */
    for (size_t i = 0; i < count; ++i) {
        if (in_degree[i] > 0) {
            order.push_back(i);
            out_cycle.push_back(plugins[i].first);
        }
    }

    return order;
}
