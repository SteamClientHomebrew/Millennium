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

#pragma once
#include <string>
#include <utility>
#include <vector>

namespace plugin_deps
{

/**
 * @brief Compute a dependency-first load order for a list of plugins.
 *
 * Each input pair is a plugin name and its dependency specs as written in
 * plugin.json ("name" or "name@<range>"; anything after the first '@' is
 * ignored here). Edges are only created between names present in the input,
 * so unknown dependencies never affect the order.
 *
 * The sort is stable: plugins with no ordering constraint between them keep
 * their original relative order, and a list without any dependencies comes
 * back in its original order. If two plugins share a name, only the first
 * occurrence is considered a dependency target.
 *
 * Plugins that are part of a dependency cycle are appended at the end in
 * their original order, and their names are written to `out_cycle` so the
 * caller can warn about them.
 *
 * @param plugins The plugin names with their dependency specs.
 * @param out_cycle Receives the names of plugins involved in a cycle.
 * @return Indices into `plugins` in load order.
 */
std::vector<size_t> resolve_load_order(const std::vector<std::pair<std::string, std::vector<std::string>>>& plugins, std::vector<std::string>& out_cycle);

} // namespace plugin_deps
