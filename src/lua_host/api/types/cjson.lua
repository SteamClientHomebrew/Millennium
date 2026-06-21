---@meta

---@class cjson
---@field null lightuserdata # Represents JSON null values
---@field _NAME string # Module name
---@field _VERSION string # Module version
local cjson = {}

---Encode a Lua value as a JSON string
---@param value any # The Lua value to encode
---@return string # JSON string representation
function cjson.encode(value) end

---Decode a JSON string into a Lua value
---@param json_string string # The JSON string to decode
---@return any # The decoded Lua value
function cjson.decode(json_string) end

---Configure sparse array encoding behavior
---@param convert? boolean|integer # Convert sparse arrays to objects (default: false)
---@param ratio? integer # Sparseness ratio threshold (default: 2)
---@param safe? integer # Always use array when max index <= safe (default: 10)
---@return boolean|string, integer, integer # Current settings
function cjson.encode_sparse_array(convert, ratio, safe) end

---Configure maximum encoding depth
---@param depth? integer # Maximum nesting depth (1-2147483647, default: 1000)
---@return integer # Current maximum depth
function cjson.encode_max_depth(depth) end

---Configure maximum decoding depth
---@param depth? integer # Maximum nesting depth (1-2147483647, default: 1000)
---@return integer # Current maximum depth
function cjson.decode_max_depth(depth) end

---Configure number precision for encoding
---@param precision? integer # Decimal precision (1-14, default: 14)
---@return integer # Current precision setting
function cjson.encode_number_precision(precision) end

---Configure encoding buffer persistence
---@param keep? boolean # Keep buffer between calls (default: true)
---@return boolean # Current buffer setting
function cjson.encode_keep_buffer(keep) end

---Configure invalid number encoding behavior
---@param setting? boolean|string # "off", "on", "null", or boolean (default: "off")
---@return boolean|string # Current setting
function cjson.encode_invalid_numbers(setting) end

---Configure invalid number decoding behavior
---@param setting? boolean # Allow invalid numbers during decoding (default: true)
---@return boolean # Current setting
function cjson.decode_invalid_numbers(setting) end

---Create a new CJSON instance
---@return cjson # New CJSON instance with default configuration
function cjson.new() end

---@class cjson.safe : cjson
---Safe version of CJSON that returns nil, error_message on failure
local cjson_safe = {}

---Safely encode a Lua value as JSON
---@param value any # The Lua value to encode
---@return string|nil # JSON string on success, nil on failure
---@return string? # Error message if encoding failed
function cjson_safe.encode(value) end

---Safely decode a JSON string
---@param json_string string # The JSON string to decode
---@return any|nil # Decoded value on success, nil on failure
---@return string? # Error message if decoding failed
function cjson_safe.decode(json_string) end

---Create a new safe CJSON instance
---@return cjson.safe # New safe CJSON instance
function cjson_safe.new() end

-- Copy other methods from regular cjson (these don't throw errors)
cjson_safe.encode_sparse_array = cjson.encode_sparse_array
cjson_safe.encode_max_depth = cjson.encode_max_depth
cjson_safe.decode_max_depth = cjson.decode_max_depth
cjson_safe.encode_number_precision = cjson.encode_number_precision
cjson_safe.encode_keep_buffer = cjson.encode_keep_buffer
cjson_safe.encode_invalid_numbers = cjson.encode_invalid_numbers
cjson_safe.decode_invalid_numbers = cjson.decode_invalid_numbers
cjson_safe.null = cjson.null
cjson_safe._NAME = cjson._NAME
cjson_safe._VERSION = cjson._VERSION

return cjson