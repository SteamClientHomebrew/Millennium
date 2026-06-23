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

local XOR_KEY                   = 0x4D
local PARITY_SEED               = 0xA7C3E91F
local STRIDE                    = 64
local HEADER_SIZE               = 12
local ENTRY_SIZE                = 14
local FLAG_OBF                  = 0x40
local FLAG_COMP                 = 0x80
local FNV_BASIS                 = 2166136261
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
		assert(#woven - pos + 1 >= 10, "truncated parity stream at block " .. idx)
		local de     = math.min(pos + STRIDE - 1, #woven - 10)
		local chunk  = woven:sub(pos, de)
		local pb     = woven:sub(de + 1, de + 10)

		local s_xor  = u32_le(pb, 1)
		local s_roll = u32_le(pb, 5)
		local s_idx  = u16_le(pb, 9)

		local xc     = 0
		for j = 1, #chunk do xc = bxor32(xc, chunk:byte(j)) end

		local e_roll = fnv1a_u32(bxor32(rolling, xc), xc)

		assert(s_xor == xc, "parity xor mismatch at block " .. idx)
		assert(s_roll == e_roll, "parity chain broken at block " .. idx)
		assert(s_idx == idx, "parity block reordered at block " .. idx)

		out[#out + 1] = chunk
		rolling, idx, pos = e_roll, idx + 1, de + 11
	end
	return table.concat(out)
end

local function decompress(data)
	assert(type(MILLENNIUM_DECOMPRESS) == "function", "MILLENNIUM_DECOMPRESS global not found")
	return MILLENNIUM_DECOMPRESS(data)
end

local function decode_section(raw, flags)
	local d = (flags % 128 >= 64) and xor_decode(raw) or raw
	d = strip_parity(d)
	if math.floor(flags / 128) % 2 == 1 then
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
	local f = assert(io.open(path, "rb"), "cannot open " .. path)
	local bytes = f:read("*a"); f:close()

	local shim_len = u32_le(bytes, 1)
	local plg      = 4 + shim_len + 1

	assert(bytes:sub(plg, plg + 3) == "STAR", "invalid .star magic")

	local ver_maj       = bytes:byte(plg + 4)
	local ver_min       = bytes:byte(plg + 5)
	local section_count = bytes:byte(plg + 6)

	local sections, tbl = {}, plg + HEADER_SIZE
	for i = 0, section_count - 1 do
		local b = tbl + i * ENTRY_SIZE
		sections[#sections + 1] = {
			id     = bytes:byte(b),
			flags  = bytes:byte(b + 1),
			offset = u32_le(bytes, b + 2),
			length = u32_le(bytes, b + 6),
			crc32  = u32_le(bytes, b + 10),
		}
	end

	local function read_section(id)
		for _, s in ipairs(sections) do
			if s.id == id then
				local raw = bytes:sub(plg + s.offset, plg + s.offset + s.length - 1)
				return decode_section(raw, s.flags)
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
