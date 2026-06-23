/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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
#include "millennium/plugin_manager.h"
#include <filesystem>
#include <optional>

enum class star_js_section
{
    frontend,
    webkit
};

std::optional<plugin_manager::plugin_t> parse_star_file(const std::filesystem::path& star_path);
std::string star_read_javascript(const std::filesystem::path& star_path, star_js_section section);

inline std::vector<uint8_t> star_decompress(const uint8_t* src, size_t src_len)
{
    if (src_len < 4) throw std::runtime_error("star_decompress: input too short");

    const uint32_t orig_size = static_cast<uint32_t>(src[0]) | (static_cast<uint32_t>(src[1]) << 8) | (static_cast<uint32_t>(src[2]) << 16) | (static_cast<uint32_t>(src[3]) << 24);

    static constexpr uint32_t STAR_DECOMPRESS_MAX = 64u * 1024u * 1024u;
    if (orig_size > STAR_DECOMPRESS_MAX) throw std::runtime_error("star_decompress: decompressed size exceeds 64 MB limit");

    std::vector<uint8_t> out(orig_size);

    const uint8_t* ip = src + 4;
    const uint8_t* ip_end = src + src_len;
    uint8_t* op = out.data();
    uint8_t* const op_end = out.data() + orig_size;

    while (ip < ip_end) {
        const uint8_t token = *ip++;

        size_t lit_len = static_cast<size_t>(token >> 4);
        if (lit_len == 15) {
            while (ip < ip_end) {
                const uint8_t b = *ip++;
                lit_len += b;
                if (b != 255) break;
            }
        }
        if (op + lit_len > op_end || ip + lit_len > ip_end) throw std::runtime_error("star_decompress: literal overflow");
        std::memcpy(op, ip, lit_len);
        op += lit_len;
        ip += lit_len;

        if (ip >= ip_end) break;

        if (ip + 2 > ip_end) throw std::runtime_error("star_decompress: truncated match offset");
        const size_t offset = static_cast<size_t>(ip[0]) | (static_cast<size_t>(ip[1]) << 8);
        ip += 2;
        if (offset == 0) throw std::runtime_error("star_decompress: zero match offset");

        size_t match_len = static_cast<size_t>(token & 0xFu) + 4u;
        if ((token & 0xFu) == 15u) {
            while (ip < ip_end) {
                const uint8_t b = *ip++;
                match_len += b;
                if (b != 255) break;
            }
        }
        if (op + match_len > op_end) throw std::runtime_error("star_decompress: match overflow");

        const uint8_t* match_src = op - offset;
        if (match_src < out.data()) throw std::runtime_error("star_decompress: invalid match offset");

        for (size_t i = 0; i < match_len; ++i)
            op[i] = match_src[i];
        op += match_len;
    }

    out.resize(static_cast<size_t>(op - out.data()));
    return out;
}
