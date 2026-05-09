#pragma once

#include <filesystem>
#include <string_view>

enum class http_code
{
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503
};

namespace mime
{
// clang-format off
enum class file_type
{
    CSS, JS, JSON, PY,
    TTF, OTF, WOFF, WOFF2,
    PNG, JPEG, JPG, GIF, WEBP, SVG,
    HTML,
    UNKNOWN
};
// clang-format on

struct file_type_t
{
    file_type type;
    std::string_view extension;
    std::string_view mime;
    bool binary;
};

static constexpr file_type_t file_types[] = {
    { file_type::CSS,   ".css",   "text/css",               false },
    { file_type::JS,    ".js",    "application/javascript", false },
    { file_type::JSON,  ".json",  "application/json",       false },
    { file_type::PY,    ".py",    "text/x-python",          false },

    { file_type::TTF,   ".ttf",   "font/ttf",               true  },
    { file_type::OTF,   ".otf",   "font/otf",               true  },
    { file_type::WOFF,  ".woff",  "font/woff",              true  },
    { file_type::WOFF2, ".woff2", "font/woff2",             true  },

    { file_type::PNG,   ".png",   "image/png",              true  },
    { file_type::JPEG,  ".jpeg",  "image/jpeg",             true  },
    { file_type::JPG,   ".jpg",   "image/jpeg",             true  },
    { file_type::GIF,   ".gif",   "image/gif",              true  },
    { file_type::WEBP,  ".webp",  "image/webp",             true  },
    { file_type::SVG,   ".svg",   "image/svg+xml",          true  },

    { file_type::HTML,  ".html",  "text/html",              false },
};

[[nodiscard]]
inline file_type get_file_type(const std::filesystem::path& path)
{
    const auto ext = path.extension().string();
    for (const auto& entry : file_types)
        if (ext == entry.extension) return entry.type;

    return file_type::UNKNOWN;
}

[[nodiscard]]
inline std::string_view get_mime_str(file_type type)
{
    for (const auto& entry : file_types)
        if (entry.type == type) return entry.mime;

    return "text/plain";
}

[[nodiscard]]
inline constexpr bool is_bin_file(file_type type)
{
    for (const auto& entry : file_types)
        if (entry.type == type) return entry.binary;

    return false;
}
} // namespace mime
