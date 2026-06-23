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

#include "millennium/star_parser.h"
#include "millennium/logger.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <openssl/evp.h>
#include <stdexcept>
#include <vector>

static constexpr uint8_t STAR_MAGIC[4] = { 'S', 'T', 'A', 'R' };
static constexpr uint32_t STAR_PARITY_SEED = 0xA7C3E91Fu;
static constexpr size_t STAR_STRIDE = 64;
static constexpr uint8_t STAR_XOR_KEY = 0x4D;
static constexpr uint8_t FLAG_OBFUSCATED = 0x40;
static constexpr uint8_t FLAG_COMPRESSED = 0x80;
static constexpr uint8_t STAR_FLAG_SIGNED = 0x01;

// clang-format off
static constexpr uint8_t STARLIGHT_PUBLIC_KEY[32] = {
    0x5d, 0x59, 0x4a, 0x76, 0x85, 0x70, 0x4c, 0x2d,
    0xea, 0xda, 0x65, 0x95, 0x40, 0xf5, 0x7a, 0x24,
    0xce, 0x5d, 0x65, 0x13, 0xf9, 0x88, 0x30, 0xaa,
    0x37, 0x1f, 0xfb, 0x10, 0x77, 0xf4, 0x69, 0xfb
};
// clang-format on

static constexpr uint8_t SEC_METADATA = 0x01;
static constexpr uint8_t SEC_BACKEND = 0x02;
static constexpr uint8_t SEC_FRONTEND = 0x03;
static constexpr uint8_t SEC_WEBKIT = 0x04;

static constexpr size_t STAR_HEADER_SIZE = 12;
static constexpr size_t STAR_ENTRY_SIZE = 14;

struct star_section_t
{
    uint8_t id;
    uint8_t flags;
    uint32_t offset;
    uint32_t length;
    uint32_t crc32;
};

struct star_sub_entry_t
{
    std::string name;
    std::vector<uint8_t> data;
};

static uint32_t read_u32_le(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

static uint16_t read_u16_le(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static void star_xor_decode(std::vector<uint8_t>& data)
{
    for (size_t i = 0; i < data.size(); ++i)
        data[i] ^= STAR_XOR_KEY ^ static_cast<uint8_t>(i & 0xFF);
}

static uint32_t fnv1a_step(uint32_t hash, uint8_t byte)
{
    return (hash ^ static_cast<uint32_t>(byte)) * 0x01000193u;
}

static uint32_t fnv1a_hash_bytes(const uint8_t* data, size_t len)
{
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; ++i)
        h = fnv1a_step(h, data[i]);
    return h;
}

static bool verify_ed25519_signature(const uint8_t* message, size_t message_len, const uint8_t* signature, const uint8_t* public_key)
{
    EVP_PKEY* pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, public_key, 32);
    if (!pkey) return false;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }

    const bool ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1 && EVP_DigestVerify(ctx, signature, 64, message, message_len) == 1;

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

static uint32_t fnv1a_u32(uint32_t state, uint32_t value)
{
    state = fnv1a_step(state, static_cast<uint8_t>(value & 0xFF));
    state = fnv1a_step(state, static_cast<uint8_t>((value >> 8) & 0xFF));
    state = fnv1a_step(state, static_cast<uint8_t>((value >> 16) & 0xFF));
    state = fnv1a_step(state, static_cast<uint8_t>((value >> 24) & 0xFF));
    return state;
}

static std::vector<uint8_t> star_strip_parity(const std::vector<uint8_t>& woven)
{
    if (woven.empty()) return {};

    std::vector<uint8_t> out;
    out.reserve(woven.size());

    uint32_t rolling = STAR_PARITY_SEED;
    uint32_t expected_idx = 0;
    size_t pos = 0;

    while (pos < woven.size()) {
        if (woven.size() - pos <= 12) throw std::runtime_error("truncated parity stream at block " + std::to_string(expected_idx));

        size_t data_end = std::min(pos + STAR_STRIDE, woven.size() - 12);

        const uint8_t* pb = woven.data() + data_end;
        uint32_t stored_xor = read_u32_le(pb);
        uint32_t stored_roll = read_u32_le(pb + 4);
        uint32_t stored_index = read_u32_le(pb + 8);

        uint32_t xor_check = 0;
        for (size_t i = pos; i < data_end; ++i)
            xor_check ^= static_cast<uint32_t>(woven[i]);

        uint32_t expected_roll = fnv1a_u32(rolling ^ xor_check, xor_check);

        if (stored_xor != xor_check) throw std::runtime_error("parity xor mismatch at block " + std::to_string(expected_idx));
        if (stored_roll != expected_roll) throw std::runtime_error("parity chain broken at block " + std::to_string(expected_idx));
        if (stored_index != expected_idx) throw std::runtime_error("parity block reordered at block " + std::to_string(expected_idx));

        out.insert(out.end(), woven.begin() + pos, woven.begin() + data_end);
        rolling = expected_roll;
        ++expected_idx;
        pos = data_end + 12;
    }

    return out;
}

static std::vector<uint8_t> star_decode_section(const std::vector<uint8_t>& file_data, size_t star_start, const star_section_t& sec)
{
    if (static_cast<size_t>(sec.offset) > file_data.size() - star_start)
        throw std::runtime_error("section 0x" + std::to_string(sec.id) + " offset overflow");
    size_t abs_start = star_start + sec.offset;
    if (static_cast<size_t>(sec.length) > file_data.size() - abs_start)
        throw std::runtime_error("section 0x" + std::to_string(sec.id) + " extends beyond file");

    std::vector<uint8_t> blob(file_data.begin() + abs_start, file_data.begin() + abs_start + sec.length);

    if (sec.flags & FLAG_OBFUSCATED) star_xor_decode(blob);

    auto stripped = star_strip_parity(blob);

    if (sec.flags & FLAG_COMPRESSED) return star_decompress(stripped.data(), stripped.size());

    return stripped;
}

static std::vector<star_sub_entry_t> star_parse_sub_entries(const std::vector<uint8_t>& data)
{
    if (data.size() < 4) throw std::runtime_error("sub-entry table too short");

    uint32_t count = read_u32_le(data.data());
    std::vector<star_sub_entry_t> entries;
    entries.reserve(count);
    size_t pos = 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 6 > data.size()) throw std::runtime_error("sub-entry " + std::to_string(i) + " header truncated");

        uint16_t name_len = read_u16_le(data.data() + pos);
        uint32_t data_len = read_u32_le(data.data() + pos + 2);
        pos += 6;

        if (pos + name_len + data_len > data.size()) throw std::runtime_error("sub-entry " + std::to_string(i) + " data truncated");

        std::string name(data.begin() + pos, data.begin() + pos + name_len);
        std::vector<uint8_t> entry_data(data.begin() + pos + name_len, data.begin() + pos + name_len + data_len);
        entries.push_back({ std::move(name), std::move(entry_data) });
        pos += name_len + data_len;
    }

    return entries;
}

static std::string bytes_to_string(const std::vector<uint8_t>& data)
{
    return std::string(data.begin(), data.end());
}

static std::string extract_primary_entry(const std::vector<star_sub_entry_t>& entries, const std::string& preferred_name)
{
    for (const auto& e : entries)
        if (e.name == preferred_name) return bytes_to_string(e.data);
    for (const auto& e : entries)
        if (!e.name.empty() && e.name.back() != '/') return bytes_to_string(e.data);
    return {};
}

static bool star_is_trusted(const uint8_t* star_hdr, const std::vector<uint8_t>& data)
{
    const uint8_t star_flags = star_hdr[7];

    bool is_trusted = false;
    if (star_flags & STAR_FLAG_SIGNED) {
        constexpr size_t SIG_LEN = 64;
        if (data.size() >= SIG_LEN) {
            const size_t payload_len = data.size() - SIG_LEN;
            const uint8_t* signature = data.data() + payload_len;
            is_trusted = verify_ed25519_signature(data.data(), payload_len, signature, STARLIGHT_PUBLIC_KEY);
        }
    }

    return is_trusted;
}

std::optional<plugin_manager::plugin_t> parse_star_file(const std::filesystem::path& star_path)
{
    size_t star_start = 0;
    std::vector<star_section_t> sections;

    std::ifstream f(star_path, std::ios::binary);
    if (!f) return std::nullopt;

    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    if (file_data.size() < 8) {
        LOG_ERROR("star: {} is too short to be a valid .star file", star_path.string());
        return std::nullopt;
    }

    if (std::memcmp(file_data.data(), STAR_MAGIC, 4) != 0) {
        // Shimmed format: [shim_len u32][raw_shim][StarHeader]
        const uint32_t shim_len = read_u32_le(file_data.data());
        if (static_cast<size_t>(shim_len) > file_data.size() - 4) {
            LOG_ERROR("star: {} shim prefix extends beyond file", star_path.string());
            return std::nullopt;
        }
        star_start = 4 + static_cast<size_t>(shim_len);

        if (star_start + STAR_HEADER_SIZE > file_data.size()) {
            LOG_ERROR("star: {} shim prefix extends beyond file", star_path.string());
            return std::nullopt;
        }
    }

    if (std::memcmp(file_data.data() + star_start, STAR_MAGIC, 4) != 0) {
        LOG_ERROR("star: {} has invalid magic bytes", star_path.string());
        return std::nullopt;
    }

    const uint8_t* star_hdr = file_data.data() + star_start;

    const bool is_trusted = star_is_trusted(star_hdr, file_data);

    // Verify shim integrity when a shim prefix is present (hash covers raw shim bytes).
    if (star_start > 0) {
        const uint32_t stored_hash = read_u32_le(star_hdr + 8);
        const uint32_t shim_len = read_u32_le(file_data.data());
        const uint32_t computed = fnv1a_hash_bytes(file_data.data() + 4, shim_len);

        if (computed != stored_hash) {
            LOG_ERROR("star: {} shim integrity check failed (stored {:08x}, computed {:08x}) — file may be tampered", star_path.string(), stored_hash, computed);
            return std::nullopt;
        }
    }

    uint8_t section_count = star_hdr[6];
    size_t table_end = STAR_HEADER_SIZE + static_cast<size_t>(section_count) * STAR_ENTRY_SIZE;

    if (star_start + table_end > file_data.size()) {
        LOG_ERROR("star: {} section table extends beyond file", star_path.string());
        return std::nullopt;
    }

    sections.reserve(section_count);

    for (uint8_t i = 0; i < section_count; ++i) {
        const uint8_t* e = star_hdr + STAR_HEADER_SIZE + i * STAR_ENTRY_SIZE;
        sections.push_back({ e[0], e[1], read_u32_le(e + 2), read_u32_le(e + 6), read_u32_le(e + 10) });
    }

    auto find_section = [&](uint8_t id) -> const star_section_t*
    {
        for (auto& s : sections)
            if (s.id == id) return &s;
        return nullptr;
    };

    const star_section_t* meta_sec = find_section(SEC_METADATA);
    if (!meta_sec) {
        LOG_ERROR("star: {} has no metadata section", star_path.string());
        return std::nullopt;
    }

    nlohmann::json metadata;
    try {
        auto raw = star_decode_section(file_data, star_start, *meta_sec);
        metadata = nlohmann::json::from_msgpack(raw, /* strict */ true, /* allow_exceptions */ false);
    } catch (const std::exception& ex) {
        LOG_ERROR("star: {} metadata decode failed: {}", star_path.string(), ex.what());
        return std::nullopt;
    }

    if (!metadata.is_object() || !metadata.contains("id") || !metadata.contains("name")) {
        LOG_ERROR("star: {} metadata is missing required fields (id, name)", star_path.string());
        return std::nullopt;
    }

    const std::string plugin_id = metadata["id"].get<std::string>();
    const std::string plugin_name = metadata["name"].get<std::string>();

    bool has_backend = find_section(SEC_BACKEND) != nullptr;

    bool has_frontend_js = find_section(SEC_FRONTEND) != nullptr;
    bool has_webkit_js = find_section(SEC_WEBKIT) != nullptr;

    nlohmann::json plugin_json = {
        { "name", plugin_id },
        { "common_name", plugin_name },
        { "description", metadata.value("description", std::string{}) },
        { "author", metadata.value("author", std::string{}) },
        { "version", metadata.value("version", std::string{}) },
        { "backendType", "lua" },
        { "useBackend", has_backend },
        { "trusted", is_trusted }
    };

    plugin_manager::plugin_t plugin;
    plugin.plugin_name = plugin_id;
    plugin.plugin_json = std::move(plugin_json);
    plugin.plugin_base_dir = star_path;
    plugin.format = plugin_manager::plugin_format::star;
    plugin.is_trusted = is_trusted;
    plugin.has_frontend_js = has_frontend_js;
    plugin.has_webkit_js = has_webkit_js;
    plugin.backend_entry = metadata.value("entry", std::string{});

    if (has_backend && plugin.backend_entry.empty()) {
        LOG_ERROR("star: {} has a backend section but metadata is missing the 'entry' field", star_path.string());
        return std::nullopt;
    }

    return plugin;
}

std::string star_read_javascript(const std::filesystem::path& star_path, star_js_section section)
{
    std::ifstream f(star_path, std::ios::binary);
    if (!f) return {};

    std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    size_t star_start = 0;
    if (file_data.size() < 8) return {};
    if (std::memcmp(file_data.data(), STAR_MAGIC, 4) != 0) {
        const uint32_t shim_len = read_u32_le(file_data.data());
        if (static_cast<size_t>(shim_len) > file_data.size() - 4) return {};
        star_start = 4 + static_cast<size_t>(shim_len);
    }

    if (star_start + STAR_HEADER_SIZE > file_data.size()) return {};

    const uint8_t* star_hdr = file_data.data() + star_start;
    const uint8_t sec_count = star_hdr[6];
    const size_t table_end = STAR_HEADER_SIZE + static_cast<size_t>(sec_count) * STAR_ENTRY_SIZE;

    if (star_start + table_end > file_data.size()) return {};

    const uint8_t target_id = (section == star_js_section::frontend) ? SEC_FRONTEND : SEC_WEBKIT;

    for (uint8_t i = 0; i < sec_count; ++i) {
        const uint8_t* e = star_hdr + STAR_HEADER_SIZE + i * STAR_ENTRY_SIZE;
        if (e[0] != target_id) continue;
        star_section_t sec{ e[0], e[1], read_u32_le(e + 2), read_u32_le(e + 6), read_u32_le(e + 10) };
        try {
            auto decoded = star_decode_section(file_data, star_start, sec);
            auto entries = star_parse_sub_entries(decoded);
            return extract_primary_entry(entries, "bundle.js");
        } catch (const std::exception& ex) {
            LOG_ERROR("star: {} js extract failed: {}", star_path.string(), ex.what());
        }
        break;
    }
    return {};
}
