/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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

#include "millennium/plugin_webkit_store.h"

std::vector<plugin_webkit_store::item> plugin_webkit_store::get()
{
    std::lock_guard<std::mutex> lock(m_accessMutex);
    return m_webkit_store;
}

void plugin_webkit_store::add(const item& webkit_item)
{
    std::lock_guard<std::mutex> lock(m_accessMutex);
    m_webkit_store.push_back(webkit_item);
}

bool plugin_webkit_store::remove(const item& webkit_item)
{
    std::lock_guard<std::mutex> lock(m_accessMutex);
    auto it = std::find(m_webkit_store.begin(), m_webkit_store.end(), webkit_item);
    if (it != m_webkit_store.end()) {
        m_webkit_store.erase(it);
        return true;
    }
    return false;
}

void plugin_webkit_store::clear()
{
    std::lock_guard<std::mutex> lock(m_accessMutex);
    m_webkit_store.clear();
}