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

#include "instrumentation/patch_registry.h"
#include <mutex>
#include <unordered_map>

static std::mutex                                         g_mu;
static std::unordered_map<std::string, std::vector<PatchEntry>> g_patches;

void patch_registry_set(const std::string& plugin, std::vector<PatchEntry> patches)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_patches[plugin] = std::move(patches);
}

void patch_registry_remove(const std::string& plugin)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_patches.erase(plugin);
}

nlohmann::json patch_registry_to_json()
{
    using json = nlohmann::json;
    json list  = json::array();

    std::lock_guard<std::mutex> lock(g_mu);
    for (const auto& [plugin, entries] : g_patches) {
        for (const auto& e : entries) {
            json transforms = json::array();
            for (const auto& t : e.transforms) {
                transforms.push_back({ { "match", t.match }, { "replace", t.replace } });
            }
            list.push_back({
                { "plugin",     e.plugin              },
                { "file",       e.file                },
                { "find",       e.find                },
                { "transforms", std::move(transforms) },
            });
        }
    }

    return list;
}
