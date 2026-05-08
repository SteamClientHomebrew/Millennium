-- If you're actually reading this find more interesting hobbies ;)

local utils = require('utils')

-- numeric utilities
assert(utils.round(1.4) == 1)
assert(utils.round(1.5) == 2)
assert(utils.round(-0.5) == 0 or utils.round(-0.5) == -1, 'round half-to-even or away-from-zero accepted')
assert(utils.clamp(5, 0, 10) == 5, 'clamp in range')
assert(utils.clamp(-1, 0, 10) == 0, 'clamp below')
assert(utils.clamp(11, 0, 10) == 10, 'clamp above')
assert(utils.sign(5) == 1)
assert(utils.sign(-5) == -1)
assert(utils.sign(0) == 0)
assert(math.abs(utils.lerp(0, 10, 0.5) - 5) < 1e-9, 'lerp midpoint')

-- random_int / random_float
utils.random_seed(42)
for _ = 1, 20 do
    local n = utils.random_int(1, 10)
    assert(n >= 1 and n <= 10, 'random_int should respect bounds, got ' .. tostring(n))
    local f = utils.random_float(0, 1)
    assert(f >= 0 and f <= 1, 'random_float should respect bounds, got ' .. tostring(f))
end

-- string utilities
assert(utils.trim('  hi  ') == 'hi')
assert(utils.ltrim('  hi  ') == 'hi  ')
assert(utils.rtrim('  hi  ') == '  hi')
assert(utils.startswith('hello world', 'hello'))
assert(utils.endswith('hello world', 'world'))
assert(utils.startswith('hello', 'world') == false)

local parts = utils.split('a,b,c', ',')
assert(#parts == 3, 'split count')
assert(parts[1] == 'a' and parts[2] == 'b' and parts[3] == 'c', 'split values')

assert(utils.join({ 'a', 'b', 'c' }, '-') == 'a-b-c', 'join')

-- table utils
assert(utils.contains({ 1, 2, 3 }, 2))
assert(utils.contains({ 1, 2, 3 }, 4) == false)

local sliced = utils.slice({ 'a', 'b', 'c', 'd' }, 2, 3)
assert(#sliced == 2, 'slice count, got ' .. #sliced)
assert(sliced[1] == 'b' and sliced[2] == 'c', 'slice values')

local keys = utils.keys({ a = 1, b = 2 })
table.sort(keys)
assert(#keys == 2, 'keys count')
assert(keys[1] == 'a' and keys[2] == 'b', 'keys values')

local vals = utils.values({ 1, 2, 3 })
assert(#vals == 3, 'values count')

local merged = utils.merge({ a = 1 }, { b = 2 })
assert(merged.a == 1 and merged.b == 2, 'merge')

-- encoding
assert(utils.base64_encode('hello') == 'aGVsbG8=', 'base64_encode hello')
-- url_encode emits lowercase hex (matches RFC 3986 either-case allowance, and the implementation uses std::hex).
assert(utils.url_encode('hello world&x=1') == 'hello%20world%26x%3d1', 'url_encode')
-- hex_encode: bytes -> lowercase hex string
assert(utils.hex_encode('A') == '41', 'hex_encode single byte')
assert(utils.hex_encode('hello') == '68656c6c6f', 'hex_encode multibyte')

-- to_hex / from_hex: integer <-> "0x.." string. NOT byte-string conversions.
assert(utils.to_hex(255) == '0xff', 'to_hex prefixes with 0x')
assert(utils.from_hex('ff') == 255, 'from_hex parses without prefix')
assert(utils.from_hex('0xff') == 255, 'from_hex parses with prefix')

-- UUID & hashing
local u1 = utils.uuid()
local u2 = utils.uuid()
assert(type(u1) == 'string' and #u1 > 0, 'uuid returns nonempty string')
assert(u1 ~= u2, 'two uuids should differ')

-- hash: std::hash<std::string> cast to integer
local hash1 = utils.hash('hello')
local hash2 = utils.hash('hello')
local hash3 = utils.hash('world')
assert(type(hash1) == 'number', 'hash returns an integer (std::hash<string>)')
assert(hash1 == hash2, 'hash is deterministic')
assert(hash1 ~= hash3, 'different inputs hash differently')

-- get_backend_path: reads our stubbed global from setup_lua_state
local backend = utils.get_backend_path()
assert(type(backend) == 'string', 'get_backend_path returns a string')
assert(#backend > 0, 'backend path should be the stubbed value, not empty')

-- environment variables
utils.setenv('MILLENNIUM_LUA_TEST_ENV', 'value-42')
assert(utils.getenv('MILLENNIUM_LUA_TEST_ENV') == 'value-42', 'set+get env round-trip')
