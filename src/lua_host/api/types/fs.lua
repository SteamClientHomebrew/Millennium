---@meta

---Filesystem module for file and directory operations
---@class fs
local fs = {}

---File/directory entry information
---@class fs.Entry
---@field name string Filename or directory name
---@field path string Full path
---@field is_directory boolean True if entry is a directory
---@field is_file boolean True if entry is a regular file
---@field is_symlink boolean True if entry is a symbolic link
---@field size? integer File size in bytes (only for regular files)
---@field depth? integer Depth in directory tree (only for recursive listings)

---Disk space information
---@class fs.SpaceInfo
---@field capacity integer Total disk capacity in bytes
---@field free integer Free space in bytes
---@field available integer Available space in bytes for non-privileged users

-- ============= Path Queries =============

---Check if a path exists
---@param path string Path to check
---@return boolean exists True if path exists
function fs.exists(path) end

---Check if a path is a directory
---@param path string Path to check
---@return boolean is_dir True if path is a directory
function fs.is_directory(path) end

---Check if a path is a regular file
---@param path string Path to check
---@return boolean is_file True if path is a regular file
function fs.is_file(path) end

---Check if a path is a symbolic link
---@param path string Path to check
---@return boolean is_symlink True if path is a symbolic link
function fs.is_symlink(path) end

---Check if a directory is empty
---@param path string Path to check
---@return boolean is_empty True if directory/file is empty
function fs.is_empty(path) end

-- ============= Directory Operations =============

---Create a single directory (parent must exist)
---@param path string Directory path
---@return boolean|nil success True if created, false if already exists, nil on error
---@return string? error Error message if failed
function fs.create_directory(path) end

---Create directories recursively (creates parent directories)
---@param path string Directory path
---@return boolean|nil success True if created, false if already exists, nil on error
---@return string? error Error message if failed
function fs.create_directories(path) end

---Remove a file or empty directory
---@param path string Path to remove
---@return boolean|nil success True if removed, false if doesn't exist, nil on error
---@return string? error Error message if failed
function fs.remove(path) end

---Remove a directory and all its contents recursively
---@param path string Path to remove
---@return integer|nil count Number of items removed, nil on error
---@return string? error Error message if failed
function fs.remove_all(path) end

---List contents of a directory (non-recursive)
---@param path string Directory path
---@return fs.Entry[]|nil entries Array of directory entries, nil on error
---@return string? error Error message if failed
function fs.list(path) end

---List contents of a directory recursively
---@param path string Directory path
---@return fs.Entry[]|nil entries Array of directory entries with depth info, nil on error
---@return string? error Error message if failed
function fs.list_recursive(path) end

-- ============= File Operations =============

---Copy a file or directory
---@param from string Source path
---@param to string Destination path
---@param throw_error? boolean If true, throws error on failure (default: false)
---@return boolean|nil success True if successful, nil on error
---@return string? error Error message if failed
function fs.copy(from, to, throw_error) end

---Copy a directory recursively
---@param from string Source directory path
---@param to string Destination directory path
---@param throw_error? boolean If true, throws error on failure (default: false)
---@return boolean|nil success True if successful, nil on error
---@return string? error Error message if failed
function fs.copy_recursive(from, to, throw_error) end

---Rename or move a file or directory
---@param from string Source path
---@param to string Destination path
---@param throw_error? boolean If true, throws error on failure (default: false)
---@return boolean|nil success True if successful, nil on error
---@return string? error Error message if failed
function fs.rename(from, to, throw_error) end

---Get the size of a file in bytes
---@param path string File path
---@return integer|nil size File size in bytes, nil on error
---@return string? error Error message if failed
function fs.file_size(path) end

---Get the last write time of a file as Unix timestamp
---@param path string File path
---@return integer|nil timestamp Last write time in seconds since epoch, nil on error
---@return string? error Error message if failed
function fs.last_write_time(path) end

-- ============= Path Operations =============

---Get the current working directory
---@return string|nil path Current working directory, nil on error
---@return string? error Error message if failed
function fs.current_path() end

---Set the current working directory
---@param path string New working directory
---@return boolean|nil success True if successful, nil on error
---@return string? error Error message if failed
function fs.set_current_path(path) end

---Convert a relative path to an absolute path
---@param path string Path to convert
---@return string|nil absolute Absolute path, nil on error
---@return string? error Error message if failed
function fs.absolute(path) end

---Convert a path to canonical form (resolves symlinks, relative paths)
---@param path string Path to convert
---@return string|nil canonical Canonical path, nil on error
---@return string? error Error message if failed
function fs.canonical(path) end

---Convert a path to relative form
---@param path string Path to convert
---@param base? string Base path (default: current directory)
---@return string|nil relative Relative path, nil on error
---@return string? error Error message if failed
function fs.relative(path, base) end

---Get the filename from a path
---@param path string Path
---@return string filename Filename with extension
function fs.filename(path) end

---Get the file extension from a path
---@param path string Path
---@return string extension File extension including the dot (e.g., ".txt")
function fs.extension(path) end

---Get the filename without extension (stem)
---@param path string Path
---@return string stem Filename without extension
function fs.stem(path) end

---Get the parent directory path
---@param path string Path
---@return string parent Parent directory path
function fs.parent_path(path) end

---Join multiple path components
---@param ... string Path components to join
---@return string path Joined path
function fs.join(...) end

-- ============= Space Information =============

---Get disk space information for a path
---@param path string Path to check
---@return fs.SpaceInfo|nil info Space information, nil on error
---@return string? error Error message if failed
function fs.space_info(path) end

return fs