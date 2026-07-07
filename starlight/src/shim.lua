-- ==================================================
--   _____ _ _ _             _
--  |     |_| | |___ ___ ___|_|_ _ _____
--  | | | | | | | -_|   |   | | | |     |
--  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
--
-- ==================================================
--
-- Copyright (c) 2026 Project Millennium
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

-- global injected by millennium
MILLENNIUM_DECOMPRESS           = MILLENNIUM_DECOMPRESS
local bit                       = require("bit")
local bxor                      = bit.bxor
local band                      = bit.band

local XOR_KEY                   = 0x4D
local PARITY_SEED               = 0xA7C3E91F
local STRIDE                    = 64
local HEADER_SIZE               = 12
-- NOTE TO SELF & CONTRIB: DO NOT CHANGE THIS, IT IS FINAL.
-- UNDER NO CIRCUMSTANCE SHOULD THIS CHANGE, IT WILL REQUIRE ALL PLUGINS TO BE RECOMPILED
local ENTRY_SIZE                = 256
local META_FLAG_DEFERRED        = 0x0001
local SEC_METADATA, SEC_BACKEND = 0x01, 0x02
local SEC_FRONTEND, SEC_WEBKIT  = 0x03, 0x04

local function bxor32(a, b)
    local r = bxor(a, b)
    return r < 0 and r + 4294967296 or r
end

local function u16_le(s, i)
    return s:byte(i) + s:byte(i + 1) * 256
end

local function u32_le(s, i)
    return s:byte(i) + s:byte(i + 1) * 256 + s:byte(i + 2) * 65536 + s:byte(i + 3) * 16777216
end

-- u64 via two u32 halves; doubles have 53-bit mantissa so this is safe for realistic file sizes
local function u64_le(s, i)
    return u32_le(s, i) + u32_le(s, i + 4) * 4294967296
end

local function xor_decode(s)
    local t = {}
    for i = 1, #s do
        t[i] = string.char(bxor32(bxor32(s:byte(i), XOR_KEY), (i - 1) % 256))
    end
    return table.concat(t)
end

local function fnv1a_step(hash, b)
    local h   = bxor32(hash, b)
    local hl  = h % 65536
    local hh  = math.floor(h / 65536)
    local lo  = hl * 403
    local mid = hh * 403 + hl * 256 + math.floor(lo / 65536)
    return (lo % 65536) + (mid % 65536) * 65536
end

local function fnv1a_u32(state, value)
    state = fnv1a_step(state, value % 256)
    state = fnv1a_step(state, math.floor(value / 256) % 256)
    state = fnv1a_step(state, math.floor(value / 65536) % 256)
    state = fnv1a_step(state, math.floor(value / 16777216) % 256)
    return state
end

local function strip_parity(woven)
    if #woven == 0 then return "" end
    local out, rolling, idx, pos = {}, PARITY_SEED, 0, 1
    while pos <= #woven do
        assert(#woven - pos + 1 >= 12, "truncated parity stream at block " .. idx)
        local de     = math.min(pos + STRIDE - 1, #woven - 12)
        local chunk  = woven:sub(pos, de)
        local pb     = woven:sub(de + 1, de + 12)

        local s_xor  = u32_le(pb, 1)
        local s_roll = u32_le(pb, 5)
        local s_idx  = u32_le(pb, 9)

        local xc     = 0
        for j = 1, #chunk do xc = bxor32(xc, chunk:byte(j)) end

        local e_roll = fnv1a_u32(rolling, xc)

        assert(s_xor == xc, "parity xor mismatch at block " .. idx)
        assert(s_roll == e_roll, "parity chain broken at block " .. idx)
        assert(s_idx == idx, "parity block reordered at block " .. idx)

        out[#out + 1] = chunk
        rolling, idx, pos = e_roll, idx + 1, de + 13
    end
    return table.concat(out)
end

local function decompress(data)
    assert(type(MILLENNIUM_DECOMPRESS) == "function", "MILLENNIUM_DECOMPRESS global not found")
    return MILLENNIUM_DECOMPRESS(data)
end

local function decode_section(raw, encode_flags)
    local d = (encode_flags % 128 >= 64) and xor_decode(raw) or raw
    d = strip_parity(d)
    if math.floor(encode_flags / 128) % 2 == 1 then
        d = decompress(d)
    end
    return d
end

local function parse_sub_entries(data)
    if not data or #data < 4 then return {} end
    local count, files, pos = u32_le(data, 1), {}, 5
    for _ = 1, count do
        local nl = u16_le(data, pos)
        local dl = u32_le(data, pos + 2)
        pos = pos + 6
        files[#files + 1] = {
            name = data:sub(pos, pos + nl - 1),
            data = data:sub(pos + nl, pos + nl + dl - 1),
        }
        pos = pos + nl + dl
    end
    return files
end

return function(path)
    local f        = assert(io.open(path, "rb"), "cannot open " .. path)

    local len_raw  = f:read(4)
    local shim_len = u32_le(len_raw, 1)
    f:seek("set", 4 + shim_len)
    local hdr_raw = f:read(HEADER_SIZE)
    assert(hdr_raw:sub(1, 4) == "STAR", "invalid .star magic")
    local ver_maj                 = hdr_raw:byte(5)
    local ver_min                 = hdr_raw:byte(6)
    local section_count           = hdr_raw:byte(7)

    local tbl_raw                 = f:read(section_count * ENTRY_SIZE)
    local sections, max_eager_end = {}, 0
    for i = 0, section_count - 1 do
        local b                 = i * ENTRY_SIZE + 1
        local meta_flags        = u32_le(tbl_raw, b + 4)
        local offset            = u64_le(tbl_raw, b + 8)
        local length            = u64_le(tbl_raw, b + 16)
        sections[#sections + 1] = {
            id           = tbl_raw:byte(b),
            encode_flags = tbl_raw:byte(b + 1),
            meta_flags   = meta_flags,
            offset       = offset,
            length       = length,
            crc32        = u32_le(tbl_raw, b + 24),
        }
        if band(meta_flags, META_FLAG_DEFERRED) == 0 then
            local section_end = offset + length
            if section_end > max_eager_end then
                max_eager_end = section_end
            end
        end
    end

    f:seek("set", 0)
    local bytes = f:read(4 + shim_len + max_eager_end)
    f:close()

    local plg = 4 + shim_len + 1

    local function read_section(id)
        for _, s in ipairs(sections) do
            if s.id == id then
                local start_pos = plg + s.offset
                local end_pos   = plg + s.offset + s.length - 1
                if end_pos > #bytes then return nil end
                local raw = bytes:sub(start_pos, end_pos)
                return decode_section(raw, s.encode_flags)
            end
        end
    end

    return {
        version      = { major = ver_maj, minor = ver_min },
        sections     = sections,
        metadata_raw = function() return read_section(SEC_METADATA) end,
        backend      = function() return parse_sub_entries(read_section(SEC_BACKEND)) end,
        frontend     = function() return parse_sub_entries(read_section(SEC_FRONTEND)) end,
        webkit       = function() return parse_sub_entries(read_section(SEC_WEBKIT)) end,
    }
end
