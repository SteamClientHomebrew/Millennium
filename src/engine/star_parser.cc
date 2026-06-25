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
static constexpr uint8_t SEC_ASSETS = 0x05;

/**
 * starlight flag to let millennium know *not* to read a section.
 * this has many use cases, but was implemented for plugin resources.
 * millennium shouldn't read userdata when parsing the star. If it does, and the star is
 * large: under memory pressure we might crash. its also just not needed I/O
 */
static constexpr uint32_t META_FLAG_DEFERRED = 0x0001u; ///< skip in bounded read; read on demand
static constexpr uint32_t META_FLAG_REQUIRED = 0x0002u; ///< refuse to load if section ID unknown

static constexpr uint8_t FORMAT_VERSION = 2;
static constexpr size_t STAR_HEADER_SIZE = 12;
static constexpr size_t STAR_ENTRY_SIZE = 256;
static constexpr uint32_t MAX_SHIM_SIZE = 4u * 1024u * 1024u;            // 4 MB
static constexpr uint64_t MAX_SECTION_SIZE = 256ull * 1024ull * 1024ull; // 256 MB

struct star_section_t
{
    uint8_t id;
    uint8_t encode_flags; /* COMPRESSED=0x80, OBFUSCATED=0x40 */
    uint32_t meta_flags;  /* DEFERRED=0x01, REQUIRED=0x02 */
    uint64_t offset;      /* bytes from star_start */
    uint64_t length;      /* encoded on-disk byte count */
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

static uint64_t read_u64_le(const uint8_t* p)
{
    return static_cast<uint64_t>(p[0]) | (static_cast<uint64_t>(p[1]) << 8) | (static_cast<uint64_t>(p[2]) << 16) | (static_cast<uint64_t>(p[3]) << 24) |
           (static_cast<uint64_t>(p[4]) << 32) | (static_cast<uint64_t>(p[5]) << 40) | (static_cast<uint64_t>(p[6]) << 48) | (static_cast<uint64_t>(p[7]) << 56);
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
    const size_t abs_start = star_start + static_cast<size_t>(sec.offset);

    if (sec.encode_flags == 0) {
        /* raw section, no encoding; return bytes directly */
        if (abs_start > file_data.size() || static_cast<size_t>(sec.length) > file_data.size() - abs_start) {
            throw std::runtime_error(std::format("section 0x{} out of bounds", sec.id));
        }

        return std::vector<uint8_t>(file_data.begin() + abs_start, file_data.begin() + abs_start + static_cast<size_t>(sec.length));
    }

    if (static_cast<size_t>(sec.offset) > file_data.size() - star_start) throw std::runtime_error("section 0x" + std::to_string(sec.id) + " offset overflow");
    if (static_cast<size_t>(sec.length) > file_data.size() - abs_start) throw std::runtime_error("section 0x" + std::to_string(sec.id) + " extends beyond file");

    std::vector<uint8_t> blob(file_data.begin() + abs_start, file_data.begin() + abs_start + static_cast<size_t>(sec.length));

    if (sec.encode_flags & FLAG_OBFUSCATED) star_xor_decode(blob);

    auto stripped = star_strip_parity(blob);

    if (sec.encode_flags & FLAG_COMPRESSED) return star_decompress(stripped.data(), stripped.size());

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

/**
 * helper: open a .star, read only the non-deferred sections + signature into a bounded buffer.
 * this prevents crafted large files from causing OOM when loading plugins.
 */
struct star_bounded_t
{
    size_t star_start;
    std::vector<star_section_t> sections;
    std::vector<uint8_t> file_data;
};

static std::optional<star_bounded_t> star_load_bounded(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;

    f.seekg(0, std::ios::end);
    const size_t file_size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    if (file_size < 8) return std::nullopt;

    uint8_t prefix[4];
    if (!f.read(reinterpret_cast<char*>(prefix), 4)) return std::nullopt;

    size_t star_start = 0;
    if (std::memcmp(prefix, STAR_MAGIC, 4) != 0) {
        const uint32_t shim_len = read_u32_le(prefix);
        if (shim_len > MAX_SHIM_SIZE || 4 + static_cast<size_t>(shim_len) > file_size) return std::nullopt;
        star_start = 4 + static_cast<size_t>(shim_len);
        f.seekg(static_cast<std::streamoff>(star_start));
    } else {
        f.seekg(0);
    }

    if (star_start + STAR_HEADER_SIZE > file_size) return std::nullopt;

    uint8_t hdr[STAR_HEADER_SIZE];
    if (!f.read(reinterpret_cast<char*>(hdr), STAR_HEADER_SIZE)) return std::nullopt;
    if (std::memcmp(hdr, STAR_MAGIC, 4) != 0) return std::nullopt;
    if (hdr[4] != FORMAT_VERSION) return std::nullopt;

    const uint8_t section_count = hdr[6];
    static constexpr uint8_t MAX_SECTIONS = 32;
    if (section_count > MAX_SECTIONS) return std::nullopt;

    const size_t table_bytes = static_cast<size_t>(section_count) * STAR_ENTRY_SIZE;
    if (star_start + STAR_HEADER_SIZE + table_bytes > file_size) return std::nullopt;

    std::vector<uint8_t> tbl(table_bytes);
    if (!f.read(reinterpret_cast<char*>(tbl.data()), table_bytes)) return std::nullopt;

    std::vector<star_section_t> sections;
    sections.reserve(section_count);
    uint64_t max_eager_end = 0;

    for (uint8_t i = 0; i < section_count; ++i) {
        const uint8_t* e = tbl.data() + i * STAR_ENTRY_SIZE;
        star_section_t s;
        s.id = e[0];
        s.encode_flags = e[1];
        s.meta_flags = read_u32_le(e + 4);
        s.offset = read_u64_le(e + 8);
        s.length = read_u64_le(e + 16);
        s.crc32 = read_u32_le(e + 24);

        /* unknown required section -> fuck off */
        if ((s.meta_flags & META_FLAG_REQUIRED) && s.id != SEC_METADATA && s.id != SEC_BACKEND && s.id != SEC_FRONTEND && s.id != SEC_WEBKIT && s.id != SEC_ASSETS) {
            LOG_ERROR("star: {} contains required unknown section 0x{:02x}", path.string(), s.id);
            return std::nullopt;
        }

        sections.push_back(s);

        if (!(s.meta_flags & META_FLAG_DEFERRED)) {
            if (s.length > MAX_SECTION_SIZE) {
                LOG_ERROR("star: {} section 0x{:02x} exceeds 256 MB size limit", path.string(), s.id);
                return std::nullopt;
            }
            const uint64_t end = s.offset + s.length;
            if (end > max_eager_end) max_eager_end = end;
        }
    }

    /* read only the non-deferred portion plus the trailing signature slot. */
    const size_t read_limit = std::min(file_size, star_start + static_cast<size_t>(max_eager_end) + 64u);
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> file_data(read_limit);
    if (!f.read(reinterpret_cast<char*>(file_data.data()), static_cast<std::streamsize>(read_limit))) return std::nullopt;

    return star_bounded_t{ star_start, std::move(sections), std::move(file_data) };
}

std::optional<plugin_manager::plugin_t> parse_star_file(const std::filesystem::path& star_path)
{
    auto loaded = star_load_bounded(star_path);
    if (!loaded) {
        LOG_ERROR("star: {} failed bounded load", star_path.string());
        return std::nullopt;
    }

    const size_t star_start = loaded->star_start;
    auto& file_data = loaded->file_data;
    auto& sections = loaded->sections;

    if (file_data.size() < star_start + STAR_HEADER_SIZE) return std::nullopt;
    const uint8_t* star_hdr = file_data.data() + star_start;

    const bool is_trusted = star_is_trusted(star_hdr, file_data);

    /* verify shim integrity if shim prefix is present. */
    if (star_start > 0) {
        const uint32_t stored_hash = read_u32_le(star_hdr + 8);
        const uint32_t shim_len = read_u32_le(file_data.data());
        const uint32_t computed = fnv1a_hash_bytes(file_data.data() + 4, shim_len);

        if (computed != stored_hash) {
            LOG_ERROR("star: {} shim integrity check failed (stored {:08x}, computed {:08x})", star_path.string(), stored_hash, computed);
            return std::nullopt;
        }
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
    auto loaded = star_load_bounded(star_path);
    if (!loaded) return {};

    const uint8_t target_id = (section == star_js_section::frontend) ? SEC_FRONTEND : SEC_WEBKIT;

    for (const auto& sec : loaded->sections) {
        if (sec.id != target_id) continue;
        try {
            auto decoded = star_decode_section(loaded->file_data, loaded->star_start, sec);
            auto entries = star_parse_sub_entries(decoded);
            return extract_primary_entry(entries, "bundle.js");
        } catch (const std::exception& ex) {
            LOG_ERROR("star: {} js extract failed: {}", star_path.string(), ex.what());
        }
        break;
    }
    return {};
}

std::unordered_map<std::string, AssetEntry> star_read_asset_index(const std::filesystem::path& star_path)
{
    std::unordered_map<std::string, AssetEntry> result;

    std::ifstream f(star_path, std::ios::binary);
    if (!f) return result;

    f.seekg(0, std::ios::end);
    const size_t file_size = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);

    if (file_size < 8) return result;

    uint8_t prefix[4];
    if (!f.read(reinterpret_cast<char*>(prefix), 4)) return result;

    size_t star_start = 0;
    if (std::memcmp(prefix, STAR_MAGIC, 4) != 0) {
        const uint32_t shim_len = read_u32_le(prefix);
        if (shim_len > MAX_SHIM_SIZE || 4 + static_cast<size_t>(shim_len) > file_size) return result;
        star_start = 4 + static_cast<size_t>(shim_len);
        f.seekg(static_cast<std::streamoff>(star_start));
    } else {
        f.seekg(0);
    }

    if (star_start + STAR_HEADER_SIZE > file_size) return result;
    uint8_t hdr[STAR_HEADER_SIZE];
    if (!f.read(reinterpret_cast<char*>(hdr), STAR_HEADER_SIZE)) return result;
    if (std::memcmp(hdr, STAR_MAGIC, 4) != 0) return result;
    if (hdr[4] != FORMAT_VERSION) return result;

    const uint8_t section_count = hdr[6];
    if (section_count > 32) return result;

    const size_t table_bytes = static_cast<size_t>(section_count) * STAR_ENTRY_SIZE;
    if (star_start + STAR_HEADER_SIZE + table_bytes > file_size) return result;

    std::vector<uint8_t> tbl(table_bytes);
    if (!f.read(reinterpret_cast<char*>(tbl.data()), table_bytes)) return result;

    /* find assets section */
    const uint8_t* asset_entry_raw = nullptr;
    for (uint8_t i = 0; i < section_count; ++i) {
        const uint8_t* e = tbl.data() + i * STAR_ENTRY_SIZE;
        if (e[0] == SEC_ASSETS) {
            asset_entry_raw = e;
            break;
        }
    }
    if (!asset_entry_raw) return result;

    const uint8_t asset_encode_flags = asset_entry_raw[1];
    const uint64_t asset_offset = read_u64_le(asset_entry_raw + 8);
    const uint64_t asset_length = read_u64_le(asset_entry_raw + 16);

    /* assets section must be raw (encode_flags == 0) */
    if (asset_encode_flags != 0) return result;
    if (asset_length == 0) return result;

    const size_t assets_abs_start = star_start + static_cast<size_t>(asset_offset);
    if (assets_abs_start + 4 > file_size) return result;

    f.seekg(static_cast<std::streamoff>(assets_abs_start));

    uint8_t count_buf[4];
    if (!f.read(reinterpret_cast<char*>(count_buf), 4)) return result;
    const uint32_t count = read_u32_le(count_buf);

    /* walk sub-entry headers; read only header bytes, skip data bodies */
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t sub_hdr[6];
        if (!f.read(reinterpret_cast<char*>(sub_hdr), 6)) break;

        const uint16_t name_len = read_u16_le(sub_hdr);
        const uint32_t data_len = read_u32_le(sub_hdr + 2);

        /* read name */
        std::string name(name_len, '\0');
        if (!f.read(name.data(), name_len)) break;

        const size_t data_offset = static_cast<size_t>(f.tellg());

        /* first 4 bytes of the compressed block = uncompressed size (lz4 prepend) */
        size_t uncompressed_length = 0;
        if (data_len >= 4) {
            uint8_t sz_buf[4];
            if (f.read(reinterpret_cast<char*>(sz_buf), 4)) {
                uncompressed_length = static_cast<size_t>(read_u32_le(sz_buf));
                f.seekg(static_cast<std::streamoff>(data_len - 4), std::ios::cur);
            }
        } else if (data_len > 0) {
            f.seekg(static_cast<std::streamoff>(data_len), std::ios::cur);
        }

        result[name] = AssetEntry{ data_offset, static_cast<size_t>(data_len), uncompressed_length };
    }

    return result;
}
