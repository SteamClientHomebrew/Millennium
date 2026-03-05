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

#include "mep_router.h"
#include <memory>

class plugin_loader;

namespace mep
{

/**
 * I'm documenting this for Millennium contributors, this API should NOT be used by plugins
 * or anything external to Millennium - it's internal and WILL change without notice.
 *
 * Registered methods:
 *   plugin.list                — list every installed plugin (with running/author/config)
 *   plugin.get                 — fetch one plugin's metadata by name
 *   plugin.enable              — enable a plugin in config and start its backend
 *   plugin.disable             — disable a plugin in config and stop its backend
 *   plugin.start               — start a plugin backend without touching config
 *   plugin.stop                — stop a plugin backend without touching config
 *   plugin.restart             — stop then re-start a plugin backend
 *   plugin.status              — query a plugin's runtime state
 *   plugin.memory              — query Lua heap memory for one or all plugins
 *   plugin.logs                — subscribe to a plugin's log stream
 *   plugin.logs.unsubscribe    — cancel a plugin.logs subscription
 *   plugin.ffi                 — subscribe to a plugin's FFI call stream
 *   plugin.ffi.unsubscribe     — cancel a plugin.ffi subscription
 *   plugin.console             — subscribe to a plugin's console (CDP) stream
 *   plugin.console.unsubscribe — cancel a plugin.console subscription
 *   file.list                  — enumerate file patterns from loopback patch SHM
 *   file.content               — read a file's content from the filesystem
 *   millennium.version         — Millennium build metadata
 *   millennium.status          — aggregate plugin stats
 */
void register_mep_handlers(router& router, std::shared_ptr<plugin_loader> loader);
} // namespace mep
