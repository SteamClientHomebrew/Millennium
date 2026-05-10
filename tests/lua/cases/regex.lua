local regex = require('regex')

-- match: full-string match
assert(regex.match('hello', 'hello') == true, 'exact match should be true')
assert(regex.match('hello', 'h.*o') == true, 'pattern should match end-to-end')
assert(regex.match('hello', 'world') == false, 'non-match should be false')
assert(regex.match('prefix-hello-suffix', 'hello') == false, 'match is full-string, not search')

-- search: find anywhere
local s = regex.search('hello world', 'wo(%w+)')
assert(s == nil, 'search uses ECMAScript regex, not Lua patterns — %w should not match')

s = regex.search('hello world', 'wo(\\w+)')
assert(s ~= nil, 'search should find a match')
assert(s[0] == 'world', 'capture[0] is the full match, got ' .. tostring(s[0]))
assert(s[1] == 'rld', 'capture[1] is the first group, got ' .. tostring(s[1]))
assert(s.pos == 7, 'pos should be 1-based byte offset, got ' .. tostring(s.pos))
assert(s.len == 5, 'len should be match length, got ' .. tostring(s.len))

-- search: no match returns nil
assert(regex.search('hello', 'xyz') == nil, 'search miss should return nil')

-- replace: substitutes all matches
assert(regex.replace('aaa bbb aaa', 'aaa', 'X') == 'X bbb X', 'replace should substitute all matches')

-- replace: regex capture groups in replacement
assert(regex.replace('John Smith', '(\\w+) (\\w+)', '$2 $1') == 'Smith John', 'replace should support back-refs')

-- escape: backslash regex meta-characters
assert(regex.escape('1.2+3') == '1\\.2\\+3', 'escape should backslash regex meta-chars')

-- test: boolean form of search
assert(regex.test('hello', 'ell') == true, 'test should be true when pattern is found anywhere')
assert(regex.test('hello', 'xyz') == false, 'test should be false when pattern not found')

-- find_all: collects every match
local all = regex.find_all('a1 b2 c3', '(\\w)(\\d)')
assert(#all == 3, 'find_all should return one entry per match, got ' .. tostring(#all))
assert(all[1][0] == 'a1', 'first match[0]')
assert(all[1][1] == 'a', 'first match[1]')
assert(all[1][2] == '1', 'first match[2]')
assert(all[3][0] == 'c3', 'third match[0]')

-- malformed pattern: returns nil + error string instead of crashing
local result, err = regex.match('foo', '(unclosed')
assert(result == nil, 'malformed pattern: result should be nil')
assert(type(err) == 'string' and #err > 0, 'malformed pattern: error message should be non-empty string')
