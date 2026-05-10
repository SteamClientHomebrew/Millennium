local json = require('json')

-- encode/decode primitives
assert(json.encode(true) == 'true')
assert(json.encode(false) == 'false')
assert(json.encode(42) == '42')
assert(json.encode('hi') == '"hi"')

assert(json.decode('true') == true)
assert(json.decode('false') == false)
assert(json.decode('42') == 42)
assert(json.decode('"hi"') == 'hi')

-- encode arrays
local arr = json.encode({ 1, 2, 3 })
assert(arr == '[1,2,3]', 'array encode, got ' .. arr)

-- encode objects (single key for deterministic ordering)
local obj = json.encode({ name = 'foo' })
assert(obj == '{"name":"foo"}', 'object encode, got ' .. obj)

-- decode → encode round-trip
local roundtrip = json.encode(json.decode('{"k":1,"v":[true,null,"x"]}'))
local back = json.decode(roundtrip)
assert(back.k == 1)
assert(back.v[1] == true)
assert(back.v[2] == nil or back.v[2] == json.null, 'null should decode to nil or json.null sentinel')
assert(back.v[3] == 'x')

assert(json.encode(json.decode('[]')) == '{}', 'cjson encodes empty Lua table as object by default')

local ok = pcall(json.decode, '{not valid')
assert(ok == false, 'malformed JSON should raise an error')

-- nested structures
local nested = json.decode('{"a":{"b":{"c":[1,2,{"d":"deep"}]}}}')
assert(nested.a.b.c[3].d == 'deep', 'nested decode should preserve structure')
