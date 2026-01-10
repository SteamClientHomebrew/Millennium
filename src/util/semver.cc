/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
/** Helper: Validate that a string contains only allowed characters */
bool IsValidIdentifier(const std::string& id, const bool allowDash = true)
{
    if (id.empty()) return false;
    return std::ranges::all_of(id, [allowDash](const char c) {
        const auto uc = static_cast<unsigned char>(c);
        return std::isalnum(uc) || (allowDash && c == '-');
    });
}

/** Helper: Check if string has leading zeros (invalid for semver numeric parts) */
bool HasLeadingZero(const std::string& s)
{
    return s.length() > 1 && s[0] == '0';
}

/** Helper: Safe integer parsing with overflow detection */
int ParseInt(const std::string& s)
{
    if (s.empty() || !std::ranges::all_of(s, ::isdigit)) {
        throw std::invalid_argument("Invalid numeric part: " + s);
    }

    if (HasLeadingZero(s)) {
        throw std::invalid_argument("Leading zeros not allowed in numeric parts: " + s);
    }

    errno = 0;
    char* end;
    const long val = std::strtol(s.c_str(), &end, 10);

    if (errno == ERANGE || val < 0 || val > INT_MAX || *end != '\0') {
        throw std::invalid_argument("Numeric part out of range or invalid: " + s);
    }

    return static_cast<int>(val);
}

/** Helper: Split string by delimiter */
std::vector<std::string> Split(const std::string& s, const char delimiter)
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
void ValidateIdentifiers(const std::string& str, const std::string& context)
{
    if (str.empty()) {
        throw std::invalid_argument("Empty " + context + " not allowed");
    }

    const std::vector<std::string> parts = Split(str, '.');
    for (const auto& part : parts) {
        if (part.empty()) {
            throw std::invalid_argument("Empty identifier in " + context + ": " += str);
        }
        if (!IsValidIdentifier(part, true)) {
            throw std::invalid_argument("Invalid characters in " + context + " identifier: " += part);
        }
        /** Check for leading zeros in numeric identifiers (prerelease only, per spec) */
        const bool isNumeric = std::ranges::all_of(part, ::isdigit);
        if (isNumeric && HasLeadingZero(part) && context == "prerelease") {
            throw std::invalid_argument("Leading zeros not allowed in numeric prerelease identifier: " + part);
        }
    }
}
} // namespace

Semver::SemverVersion::SemverVersion(const int maj, const int min, const int pat, std::string  pre, std::string  bld)
    : major(maj), minor(min), patch(pat), prerelease(std::move(pre)), build(std::move(bld))
{
}

Semver::SemverVersion Semver::ParseSemver(const std::string& version)
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
    const size_t buildPos = core.find('+');
    if (buildPos != std::string::npos) {
        if (buildPos == core.length() - 1) {
            throw std::invalid_argument("Empty build metadata after '+'");
        }
        build = core.substr(buildPos + 1);
        core = core.substr(0, buildPos);
        ValidateIdentifiers(build, "build metadata");
    }

    /** Extract prerelease (everything after '-') */
    const size_t prereleasePos = core.find('-');
    if (prereleasePos != std::string::npos) {
        if (prereleasePos == core.length() - 1) {
            throw std::invalid_argument("Empty prerelease after '-'");
        }
        prerelease = core.substr(prereleasePos + 1);
        core = core.substr(0, prereleasePos);
        ValidateIdentifiers(prerelease, "prerelease");
    }

    /** Parse major.minor.patch */
    const std::vector<std::string> parts = Split(core, '.');

    if (parts.size() != 3) {
        throw std::invalid_argument("Semver requires exactly 3 numeric parts (major.minor.patch)");
    }

    /** Validate and parse each part */
    const int major = ParseInt(parts[0]);
    const int minor = ParseInt(parts[1]);
    const int patch = ParseInt(parts[2]);

    if (major < 0 || minor < 0 || patch < 0) {
        throw std::invalid_argument("Version numbers cannot be negative");
    }

    return Semver::SemverVersion(major, minor, patch, prerelease, build);
}

int Semver::ComparePrereleases(const std::string& pre1, const std::string& pre2)
{
    /** No prerelease has higher precedence than any prerelease */
    if (pre1.empty() && pre2.empty()) return 0;
    if (pre1.empty()) return 1;  /** 1.0.0 > 1.0.0-alpha */
    if (pre2.empty()) return -1; /** 1.0.0-alpha < 1.0.0 */

    /** Split prerelease identifiers by '.' */
    const std::vector<std::string> parts1 = Split(pre1, '.');
    const std::vector<std::string> parts2 = Split(pre2, '.');

    const size_t minSize = std::min(parts1.size(), parts2.size());

    for (size_t i = 0; i < minSize; ++i) {
        const std::string& p1 = parts1[i];
        const std::string& p2 = parts2[i];

        /** Both parts should already be validated, but check for safety */
        if (p1.empty() || p2.empty()) {
            throw std::invalid_argument("Empty prerelease identifier encountered");
        }

        const bool p1IsNumeric = std::ranges::all_of(p1, ::isdigit);
        const bool p2IsNumeric = std::ranges::all_of(p2, ::isdigit);

        if (p1IsNumeric && p2IsNumeric) {
            /** Both numeric: compare numerically
             * Use safe parsing with overflow checking */
            try {
                errno = 0;
                char* end1;
                char* end2;
                const long n1 = std::strtol(p1.c_str(), &end1, 10);
                const long n2 = std::strtol(p2.c_str(), &end2, 10);

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

int Semver::Compare(const std::string& v1, const std::string& v2)
{
    try {
        const SemverVersion ver1 = ParseSemver(v1);
        const SemverVersion ver2 = ParseSemver(v2);

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
        return ComparePrereleases(ver1.prerelease, ver2.prerelease);

    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid semver format");
    }
}