---@meta

---HTTP module for making HTTP requests using libcurl
---@class http
local http = {}

---Authentication credentials for HTTP requests
---@class HTTPAuth
---@field user string Username for authentication
---@field pass string Password for authentication

---HTTP request options
---@class HTTPOptions
---@field method? string HTTP method (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS). Default: "GET"
---@field data? string Request body data
---@field headers? table<string, string> HTTP headers as key-value pairs
---@field timeout? integer Timeout in seconds. Default: 30
---@field follow_redirects? boolean Whether to follow redirects. Default: true
---@field verify_ssl? boolean Whether to verify SSL certificates. Default: true
---@field user_agent? string User agent string. Default: "Lua-HTTP/1.0"
---@field auth? HTTPAuth Authentication credentials
---@field proxy? string Proxy URL (e.g., "http://proxy.example.com:8080")

---HTTP response object
---@class HTTPResponse
---@field status integer HTTP status code (e.g., 200, 404, 500)
---@field body string Response body content
---@field headers table<string, string> Response headers as key-value pairs

---Make a generic HTTP request with full configuration options
---@param url string The URL to request
---@param options? HTTPOptions Request configuration options
---@return HTTPResponse|nil response Response object or nil on failure
---@return string? error Error message if request failed
function http.request(url, options) end

---Make a GET request
---@param url string The URL to request
---@param options? HTTPOptions Additional request options
---@return HTTPResponse|nil response Response object or nil on failure
---@return string? error Error message if request failed
function http.get(url, options) end

---Make a POST request
---@param url string The URL to request
---@param data? string Request body data
---@param options? HTTPOptions Additional request options
---@return HTTPResponse|nil response Response object or nil on failure
---@return string? error Error message if request failed
function http.post(url, data, options) end

---Make a PUT request
---@param url string The URL to request
---@param data? string Request body data
---@param options? HTTPOptions Additional request options
---@return HTTPResponse|nil response Response object or nil on failure
---@return string? error Error message if request failed
function http.put(url, data, options) end

---Make a DELETE request
---@param url string The URL to request
---@param options? HTTPOptions Additional request options
---@return HTTPResponse|nil response Response object or nil on failure
---@return string? error Error message if request failed
function http.delete(url, options) end

return http