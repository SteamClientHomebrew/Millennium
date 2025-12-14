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

#pragma once
#include <string>

namespace Semver
{
struct SemverVersion
{
    int major;
    int minor;
    int patch;
    std::string prerelease;
    std::string build;

    SemverVersion(int maj = 0, int min = 0, int pat = 0, const std::string& pre = "", const std::string& bld = "");
};

/**
 * Parse a semantic version string into its components
 * @param version The version string to parse (e.g., "1.2.3-alpha+build")
 * @return SemverVersion struct with parsed components
 * @throws std::invalid_argument if version format is invalid
 */
SemverVersion ParseSemver(const std::string& version);

/**
 * Compare two semantic version strings
 * @param v1 First version string
 * @param v2 Second version string
 * @return -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
 * @throws std::invalid_argument if either version format is invalid
 */
int Compare(const std::string& v1, const std::string& v2);

/**
 * Compare prerelease identifiers according to semver rules
 * @param pre1 First prerelease string
 * @param pre2 Second prerelease string
 * @return -1 if pre1 < pre2, 0 if pre1 == pre2, 1 if pre1 > pre2
 */
int ComparePrereleases(const std::string& pre1, const std::string& pre2);
} // namespace Semver
