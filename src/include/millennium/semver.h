#pragma once

#include <string>
#include <vector>

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