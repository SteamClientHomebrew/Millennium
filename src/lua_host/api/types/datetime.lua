---@meta
---DateTime module for date and time manipulation
---@class datetime
local datetime = {}

---Date and time components table
---@class DateTimeComponents
---@field year integer Full year (e.g., 2025)
---@field month integer Month (1-12)
---@field day integer Day of month (1-31)
---@field hour integer Hour (0-23)
---@field minute integer Minute (0-59)
---@field second integer Second (0-59)
---@field weekday integer Day of week (0=Sunday, 6=Saturday)

---Get current time as milliseconds since epoch
---@return integer milliseconds Current time in milliseconds
function datetime.now() end

---Get current time as seconds since epoch (Unix timestamp)
---@return integer seconds Current Unix timestamp
function datetime.unix() end

---Convert Unix timestamp (seconds) to milliseconds
---@param seconds integer Unix timestamp in seconds
---@return integer milliseconds Time in milliseconds
function datetime.from_unix(seconds) end

---Convert milliseconds to Unix timestamp (seconds)
---@param milliseconds integer Time in milliseconds
---@return integer seconds Unix timestamp
function datetime.to_unix(milliseconds) end

---Format a timestamp as a string
---@param milliseconds integer Time in milliseconds since epoch
---@param format? string Format string (strftime format, default: "%Y-%m-%d %H:%M:%S")
---@param utc? boolean Use UTC time instead of local (default: false)
---@return string formatted Formatted date/time string
function datetime.format(milliseconds, format, utc) end

---Parse a date/time string into milliseconds
---@param str string Date/time string to parse
---@param format? string Format string (strftime format, default: "%Y-%m-%d %H:%M:%S")
---@return integer|nil milliseconds Parsed time in milliseconds, or nil on failure
---@return string? error Error message if parsing failed
function datetime.parse(str, format) end

---Add time to a timestamp
---@param milliseconds integer Base time in milliseconds
---@param delta integer Amount to add (can be negative)
---@param unit? string Time unit: "milliseconds", "seconds", "minutes", "hours", "days" (default: "seconds")
---@return integer milliseconds New time in milliseconds
function datetime.add(milliseconds, delta, unit) end

---Calculate difference between two timestamps
---@param milliseconds1 integer First time in milliseconds
---@param milliseconds2 integer Second time in milliseconds
---@param unit? string Result unit: "milliseconds", "seconds", "minutes", "hours", "days" (default: "seconds")
---@return integer difference Time difference in specified unit
function datetime.diff(milliseconds1, milliseconds2, unit) end

---Get date/time components from a timestamp
---@param milliseconds integer Time in milliseconds since epoch
---@param utc? boolean Use UTC time instead of local (default: false)
---@return DateTimeComponents components Table with year, month, day, hour, minute, second, weekday
function datetime.components(milliseconds, utc) end

---Create a timestamp from date/time components
---@param components DateTimeComponents Table with date/time fields
---@return integer milliseconds Time in milliseconds since epoch
function datetime.create(components) end

return datetime