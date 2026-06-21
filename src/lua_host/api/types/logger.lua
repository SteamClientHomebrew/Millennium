---@meta

---Logger module for Millennium plugin development
---Provides logging functionality with different severity levels
---@class logger
local logger = {}

---Log an informational message
---@param message string The message to log
function logger:info(message) end

---Log a warning message  
---@param message string The warning message to log
function logger:warn(message) end

---Log an error message
---@param message string The error message to log
function logger:error(message) end

return logger