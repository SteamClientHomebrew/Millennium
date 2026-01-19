/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "millennium/semver.h"
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <vector>
#include <climits>
#include <cerrno>
#include <cstdlib>

/** Helper: Validate that a string contains only allowed characters */
bool is_valid_identifier(const std::string& id, bool allowDash = true)
{
    if (id.empty()) return false;
    for (char c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && !(allowDash && c == '-')) {
            return false;
        }
    }
    return true;
}

/** Helper: Check if string has leading zeros (invalid for semver numeric parts) */
bool has_leading_zero(const std::string& s)
{
    return s.length() > 1 && s[0] == '0';
}

/** Helper: Safe integer parsing with overflow detection */
int parse_int(const std::string& s)
{
    if (s.empty() || !std::all_of(s.begin(), s.end(), ::isdigit)) {
        throw std::invalid_argument("Invalid numeric part: " + s);
    }

    if (has_leading_zero(s)) {
        throw std::invalid_argument("Leading zeros not allowed in numeric parts: " + s);
    }

    errno = 0;
    char* end;
    long val = std::strtol(s.c_str(), &end, 10);

    if (errno == ERANGE || val < 0 || val > INT_MAX || *end != '\0') {
        throw std::invalid_argument("Numeric part out of range or invalid: " + s);
    }

    return static_cast<int>(val);
}

/** Helper: Split string by delimiter */
std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end;

    while ((end = s.find(delimiter, start)) != std::string::npos) {
        parts.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(s.substr(start));
    return parts;
}

/** Helper: Validate prerelease or build metadata identifiers */
void validate(const std::string& str, const std::string& context)
{
    if (str.empty()) {
        throw std::invalid_argument("Empty " + context + " not allowed");
    }

    std::vector<std::string> parts = split(str, '.');
    for (const auto& part : parts) {
        if (part.empty()) {
            throw std::invalid_argument("Empty identifier in " + context + ": " + str);
        }
        if (!is_valid_identifier(part, true)) {
            throw std::invalid_argument("Invalid characters in " + context + " identifier: " + part);
        }
        /** Check for leading zeros in numeric identifiers (prerelease only, per spec) */
        bool isNumeric = std::all_of(part.begin(), part.end(), ::isdigit);
        if (isNumeric && has_leading_zero(part) && context == "prerelease") {
            throw std::invalid_argument("Leading zeros not allowed in numeric prerelease identifier: " + part);
        }
    }
}

semver::semver_version::semver_version(int maj, int min, int pat, const std::string& pre, const std::string& bld) : major(maj), minor(min), patch(pat), prerelease(pre), build(bld)
{
}

semver::semver_version semver::parse(const std::string& version)
{
    if (version.empty()) {
        throw std::invalid_argument("Empty version string");
    }

    /** Check for leading/trailing dots */
    if (version[0] == '.' || version.back() == '.') {
        throw std::invalid_argument("Version cannot start or end with '.'");
    }

    std::string core = version;
    std::string build;
    std::string prerelease;

    /** Extract build metadata (everything after '+') */
    size_t buildPos = core.find('+');
    if (buildPos != std::string::npos) {
        if (buildPos == core.length() - 1) {
            throw std::invalid_argument("Empty build metadata after '+'");
        }
        build = core.substr(buildPos + 1);
        core = core.substr(0, buildPos);
        validate(build, "build metadata");
    }

    /** Extract prerelease (everything after '-') */
    size_t prereleasePos = core.find('-');
    if (prereleasePos != std::string::npos) {
        if (prereleasePos == core.length() - 1) {
            throw std::invalid_argument("Empty prerelease after '-'");
        }
        prerelease = core.substr(prereleasePos + 1);
        core = core.substr(0, prereleasePos);
        validate(prerelease, "prerelease");
    }

    /** Parse major.minor.patch */
    std::vector<std::string> parts = split(core, '.');

    if (parts.size() != 3) {
        throw std::invalid_argument("Semver requires exactly 3 numeric parts (major.minor.patch)");
    }

    /** Validate and parse each part */
    int major = parse_int(parts[0]);
    int minor = parse_int(parts[1]);
    int patch = parse_int(parts[2]);

    if (major < 0 || minor < 0 || patch < 0) {
        throw std::invalid_argument("Version numbers cannot be negative");
    }

    return semver::semver_version(major, minor, patch, prerelease, build);
}

int semver::cmp_pre_release(const std::string& pre1, const std::string& pre2)
{
    /** No prerelease has higher precedence than any prerelease */
    if (pre1.empty() && pre2.empty()) return 0;
    if (pre1.empty()) return 1;  /** 1.0.0 > 1.0.0-alpha */
    if (pre2.empty()) return -1; /** 1.0.0-alpha < 1.0.0 */

    /** Split prerelease identifiers by '.' */
    std::vector<std::string> parts1 = split(pre1, '.');
    std::vector<std::string> parts2 = split(pre2, '.');

    size_t minSize = std::min(parts1.size(), parts2.size());

    for (size_t i = 0; i < minSize; ++i) {
        const std::string& p1 = parts1[i];
        const std::string& p2 = parts2[i];

        /** Both parts should already be validated, but check for safety */
        if (p1.empty() || p2.empty()) {
            throw std::invalid_argument("Empty prerelease identifier encountered");
        }

        bool p1IsNumeric = std::all_of(p1.begin(), p1.end(), ::isdigit);
        bool p2IsNumeric = std::all_of(p2.begin(), p2.end(), ::isdigit);

        if (p1IsNumeric && p2IsNumeric) {
            /** Both numeric: compare numerically
             * Use safe parsing with overflow checking */
            try {
                errno = 0;
                char* end1;
                char* end2;
                long n1 = std::strtol(p1.c_str(), &end1, 10);
                long n2 = std::strtol(p2.c_str(), &end2, 10);

                if (errno == ERANGE || n1 > INT_MAX || n2 > INT_MAX) {
                    throw std::invalid_argument("Prerelease numeric identifier overflow");
                }

                if (n1 != n2) return (n1 < n2) ? -1 : 1;
            } catch (...) {
                throw std::invalid_argument("Invalid numeric prerelease identifier");
            }
        } else if (p1IsNumeric) {
            /** Numeric identifiers have lower precedence than non-numeric */
            return -1;
        } else if (p2IsNumeric) {
            /** Non-numeric identifiers have higher precedence than numeric */
            return 1;
        } else {
            /** Both non-numeric: compare lexically */
            if (p1 != p2) return (p1 < p2) ? -1 : 1;
        }
    }

    /** If all compared parts are equal, larger set of identifiers has higher precedence */
    if (parts1.size() != parts2.size()) {
        return (parts1.size() < parts2.size()) ? -1 : 1;
    }

    return 0;
}

int semver::cmp(const std::string& v1, const std::string& v2)
{
    try {
        semver_version ver1 = parse(v1);
        semver_version ver2 = parse(v2);

        /** Compare major.minor.patch */
        if (ver1.major != ver2.major) {
            return (ver1.major < ver2.major) ? -1 : 1;
        }
        if (ver1.minor != ver2.minor) {
            return (ver1.minor < ver2.minor) ? -1 : 1;
        }
        if (ver1.patch != ver2.patch) {
            return (ver1.patch < ver2.patch) ? -1 : 1;
        }

        /**
         * If core versions are equal, compare prereleases
         * Build metadata is ignored in precedence comparison per semver spec
         */
        return cmp_pre_release(ver1.prerelease, ver2.prerelease);

    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid semver format");
    }
}
