local fs = require('fs')

-- unique scratch root:
-- os.tmpname() creates and returns a path; we use its directory + a uuid-ish name.
local function unique_dir()
    local probe = os.tmpname()
    -- on some platforms tmpname creates a 0-byte file; remove it so we can mkdir at the same path
    os.remove(probe)
    return probe .. '-millennium-fs-test-' .. tostring(math.random(1, 1e9))
end

local root = unique_dir()
assert(fs.create_directory(root), 'create_directory should return truthy on success')
assert(fs.exists(root), 'exists should be true after create')
assert(fs.is_directory(root), 'is_directory should be true for a dir')
assert(fs.is_file(root) == false, 'is_file should be false for a dir')

-- create_directories: nested
local nested = fs.join(root, 'a', 'b', 'c')
assert(fs.create_directories(nested), 'create_directories should create the full chain')
assert(fs.is_directory(nested), 'nested dir should exist')

-- path manipulation: pure functions, no I/O
assert(fs.filename('/foo/bar/baz.txt') == 'baz.txt', 'filename')
assert(fs.extension('/foo/bar/baz.txt') == '.txt', 'extension')
assert(fs.stem('/foo/bar/baz.txt') == 'baz', 'stem')
assert(fs.parent_path('/foo/bar/baz.txt') == '/foo/bar', 'parent_path')

-- list / list_recursive
local listed = fs.list(root)
assert(type(listed) == 'table', 'list should return a table')

local listed_rec = fs.list_recursive(root)
assert(type(listed_rec) == 'table', 'list_recursive should return a table')
assert(#listed_rec >= 3, 'list_recursive should return all subdirs (a, a/b, a/b/c), got ' .. #listed_rec)

-- rename
local before = fs.join(root, 'before')
local after = fs.join(root, 'after')
assert(fs.create_directory(before), 'create source for rename')
assert(fs.rename(before, after), 'rename should succeed')
assert(fs.exists(after), 'rename target should exist')
assert(fs.exists(before) == false, 'rename source should no longer exist')

-- remove / remove_all
local single = fs.join(root, 'single-file-dir')
fs.create_directory(single)
assert(fs.remove(single), 'remove on empty dir should succeed')
assert(fs.exists(single) == false, 'removed dir should not exist')

-- cleanup
assert(fs.remove_all(root) > 0, 'remove_all should report nonzero count')
assert(fs.exists(root) == false, 'root should be gone after remove_all')

-- exists on nonexistent path is false (not an error)
assert(fs.exists('/this/path/should/not/exist/anywhere') == false)
