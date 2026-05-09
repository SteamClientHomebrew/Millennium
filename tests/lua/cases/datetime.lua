local datetime = require('datetime')

-- now / unix: now should be > unix*1000 by at most a few ms; both should be plausible (after 2024)
local now_ms = datetime.now()
local unix_s = datetime.unix()
assert(type(now_ms) == 'number', 'now() should return a number, got ' .. type(now_ms))
assert(type(unix_s) == 'number', 'unix() should return a number, got ' .. type(unix_s))
assert(now_ms > 1700000000000, 'now() should be after 2023, got ' .. tostring(now_ms))
assert(unix_s > 1700000000, 'unix() should be after 2023, got ' .. tostring(unix_s))
assert(math.abs(now_ms - unix_s * 1000) < 5000, 'now() and unix()*1000 should agree within 5s')

-- from_unix / to_unix: round-trip
assert(datetime.from_unix(1700000000) == 1700000000000, 'from_unix should multiply by 1000')
assert(datetime.to_unix(1700000000000) == 1700000000, 'to_unix should divide by 1000')

-- format / parse: round-trip with the default format
local formatted = datetime.format(1700000000000, '%Y-%m-%d %H:%M:%S', true) -- UTC
assert(formatted == '2023-11-14 22:13:20', 'format default UTC should produce ISO-ish string, got ' .. formatted)

local parsed = datetime.parse('2023-11-14 22:13:20', '%Y-%m-%d %H:%M:%S')
-- parse uses local time so we can't directly compare ms across timezones; verify it's a sensible number near the same day
assert(type(parsed) == 'number', 'parse should return a number on success')
assert(math.abs(parsed - 1700000000000) < 24 * 3600 * 1000, 'parse result should be within one day of expected')

-- parse: malformed input returns nil + error
local bad, err = datetime.parse('not-a-date', '%Y-%m-%d')
assert(bad == nil, 'parse miss should return nil')
assert(type(err) == 'string', 'parse miss should return error string as second value')

-- add: arithmetic over time units
assert(datetime.add(1000, 1, 'seconds') == 2000, 'add seconds')
assert(datetime.add(1000, 1, 'minutes') == 61000, 'add minutes')
assert(datetime.add(1000, 1, 'hours') == 3601000, 'add hours')
assert(datetime.add(0, 1, 'days') == 86400000, 'add days')
assert(datetime.add(1000, 100, 'milliseconds') == 1100, 'add milliseconds')
assert(datetime.add(1000, -1, 'seconds') == 0, 'add accepts negative deltas')

-- diff: difference in arbitrary unit
assert(datetime.diff(2000, 1000, 'seconds') == 1, 'diff in seconds')
assert(datetime.diff(61000, 1000, 'minutes') == 1, 'diff in minutes')
assert(datetime.diff(86400000, 0, 'days') == 1, 'diff in days')
assert(datetime.diff(2000, 1000, 'milliseconds') == 1000, 'diff in ms')
