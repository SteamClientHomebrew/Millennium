---@meta

---Millennium module for Steam plugin development
---@class millennium
local millennium = {}

---Plugin log entry
---@class PluginLogEntry
---@field message string Base64 encoded log message
---@field level integer Log level (0=debug, 1=info, 2=warn, 3=error)

---Plugin logs collection
---@class PluginLogs
---@field name string Display name of the plugin
---@field logs PluginLogEntry[] Array of log entries for this plugin

---Called one time after your plugin has finished bootstrapping.
---Used to let Millennium know what plugins crashed/loaded etc.
---@return boolean success True if the ready message was sent successfully
function millennium.ready() end

---Add a CSS file to the browser webkit hook list
---@param moduleItem string Path to CSS file relative to steamui directory
---@param regexSelector? string Regex pattern for URL matching (default: ".*")
---@return integer moduleId Module ID for later removal, or 0 on failure
function millennium.add_browser_css(moduleItem, regexSelector) end

---Add a JavaScript file to the browser webkit hook list
---@param moduleItem string Path to JS file relative to steamui directory
---@param regexSelector? string Regex pattern for URL matching (default: ".*")
---@return integer moduleId Module ID for later removal, or 0 on failure
function millennium.add_browser_js(moduleItem, regexSelector) end

---Remove a CSS or JavaScript file using the ModuleID from add_browser_js/css
---@param moduleId integer Module ID returned from add_browser_css or add_browser_js
---@return boolean success True if module was successfully removed
function millennium.remove_browser_module(moduleId) end

---Plugin configuration sub-module (stored in config.json under plugins.<name>.config)
---@class millennium.config
millennium.config = {}

---Get a config value by key
---@param key string The config key to retrieve
---@return any value The stored value (string, number, boolean, table, or nil if not set)
---@return string|nil error Error message if the call failed
function millennium.config.get(key) end

---Set a config value by key. The change is persisted to disk and pushed to the frontend automatically.
---@param key string The config key to set
---@param value any The value to store (string, number, boolean, or table)
---@return boolean success True if the value was set successfully
---@return string|nil error Error message if the call failed
function millennium.config.set(key, value) end

---Delete a config key. The deletion is persisted and pushed to the frontend automatically.
---@param key string The config key to delete
---@return boolean success True if the key was deleted successfully
---@return string|nil error Error message if the call failed
function millennium.config.delete(key) end

---Get all config values for this plugin
---@return table config A table of all key-value pairs, or empty table if none set
---@return string|nil error Error message if the call failed
function millennium.config.get_all() end

---Register a callback for config changes (from frontend, MEP, or other sources).
---@param key_or_callback string|function If a string, only changes to this key trigger the callback. If a function, all changes trigger it.
---@param callback? function The callback function receiving (key, value). Required when first arg is a key string.
---@return function unsubscribe Call this to stop listening
function millennium.config.on_change(key_or_callback, callback) end

---Get the version of Millennium in semantic versioning format
---@return string version Millennium version string (e.g., "1.0.0")
function millennium.version() end

---Get the path to the Steam directory
---@return string steamPath Full path to Steam installation directory
function millennium.steam_path() end

---Get the path to the Millennium install directory
---@return string installPath Full path to Millennium installation directory
function millennium.get_install_path() end

---Get all current stored logs from all loaded and previously loaded plugins during this instance
---@return string logsJson JSON string containing array of PluginLogs objects
function millennium.get_plugin_logs() end

---Call a JavaScript method on the frontend
---@param methodName string Name of the method to call on the frontend
---@param params? (string|number|boolean)[] Array of parameters (only string, number, boolean supported)
---@return any result Result from the frontend method call
function millennium.call_frontend_method(methodName, params) end

---Toggle the status of a plugin (Use with caution, this is an internal function and may change without notice)
---@param pluginName string Name of the plugin to toggle
---@return any result Result of the toggle operation
function millennium.change_plugin_status(pluginName) end

---Check if a plugin is enabled (Use with caution, this is an internal function and may change without notice)
---@param pluginName string Name of the plugin to check
---@return boolean enabled True if the plugin is enabled
function millennium.is_plugin_enabled(pluginName) end

--- Compare two semantic versions against one another. Very useful when you need to conditionally add features depending on the version of Millennium.
---
--- Example: If there is a API function "new_call" introduced in Millennium 2.30.0 and up, you can check if the function is available on the users installation with
---```lua
--- millennium.cmp_version(millennium.version(), "2.30.0") >= 0
--- ```
---
---@param version1 string The first version to compare
---@param version2 string version2
---@return number status -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2, -2 if there was an error parsing or comparing versions.
function millennium.cmp_version(version1, version2) end

return millennium
