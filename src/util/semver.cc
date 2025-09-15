#include "millennium/semver.h"
#include <algorithm>
#include <stdexcept>
#include <cctype>

Semver::SemverVersion::SemverVersion(int maj, int min, int pat, const std::string& pre, const std::string& bld) : major(maj), minor(min), patch(pat), prerelease(pre), build(bld)
{
}

Semver::SemverVersion Semver::ParseSemver(const std::string& version)
{
    if (version.empty()) {
        throw std::invalid_argument("Empty version string");
    }

    std::string core = version;
    std::string build;
    std::string prerelease;

    // Extract build metadata (everything after '+')
    size_t buildPos = core.find('+');
    if (buildPos != std::string::npos) {
        build = core.substr(buildPos + 1);
        core = core.substr(0, buildPos);
    }

    // Extract prerelease (everything after '-')
    size_t prereleasePos = core.find('-');
    if (prereleasePos != std::string::npos) {
        prerelease = core.substr(prereleasePos + 1);
        core = core.substr(0, prereleasePos);
    }

    // Parse major.minor.patch
    std::vector<int> parts;
    size_t start = 0;
    size_t end;

    while ((end = core.find('.', start)) != std::string::npos) {
        std::string part = core.substr(start, end - start);
        if (part.empty() || !std::all_of(part.begin(), part.end(), ::isdigit)) {
            throw std::invalid_argument("Invalid numeric part in version");
        }
        parts.push_back(std::stoi(part));
        start = end + 1;
    }

    // Parse the last part
    std::string lastPart = core.substr(start);
    if (lastPart.empty() || !std::all_of(lastPart.begin(), lastPart.end(), ::isdigit)) {
        throw std::invalid_argument("Invalid numeric part in version");
    }
    parts.push_back(std::stoi(lastPart));

    if (parts.size() != 3) {
        throw std::invalid_argument("Semver requires exactly 3 numeric parts (major.minor.patch)");
    }

    return Semver::SemverVersion(parts[0], parts[1], parts[2], prerelease, build);
}

int Semver::ComparePrereleases(const std::string& pre1, const std::string& pre2)
{
    // No prerelease has higher precedence than any prerelease
    if (pre1.empty() && pre2.empty())
        return 0;
    if (pre1.empty())
        return 1; // 1.0.0 > 1.0.0-alpha
    if (pre2.empty())
        return -1; // 1.0.0-alpha < 1.0.0

    // Split prerelease identifiers by '.'
    auto split = [](const std::string& s) -> std::vector<std::string>
    {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end;

        while ((end = s.find('.', start)) != std::string::npos) {
            parts.push_back(s.substr(start, end - start));
            start = end + 1;
        }
        parts.push_back(s.substr(start));
        return parts;
    };

    std::vector<std::string> parts1 = split(pre1);
    std::vector<std::string> parts2 = split(pre2);

    size_t minSize = std::min(parts1.size(), parts2.size());

    for (size_t i = 0; i < minSize; ++i) {
        const std::string& p1 = parts1[i];
        const std::string& p2 = parts2[i];

        bool p1IsNumeric = std::all_of(p1.begin(), p1.end(), ::isdigit) && !p1.empty();
        bool p2IsNumeric = std::all_of(p2.begin(), p2.end(), ::isdigit) && !p2.empty();

        if (p1IsNumeric && p2IsNumeric) {
            // Both numeric: compare numerically
            int n1 = std::stoi(p1);
            int n2 = std::stoi(p2);
            if (n1 != n2)
                return (n1 < n2) ? -1 : 1;
        } else if (p1IsNumeric) {
            // Numeric identifiers have lower precedence than non-numeric
            return -1;
        } else if (p2IsNumeric) {
            // Non-numeric identifiers have higher precedence than numeric
            return 1;
        } else {
            // Both non-numeric: compare lexically
            if (p1 != p2)
                return (p1 < p2) ? -1 : 1;
        }
    }

    // If all compared parts are equal, larger set of identifiers has higher precedence
    if (parts1.size() != parts2.size()) {
        return (parts1.size() < parts2.size()) ? -1 : 1;
    }

    return 0;
}

int Semver::Compare(const std::string& v1, const std::string& v2)
{
    try {
        SemverVersion ver1 = ParseSemver(v1);
        SemverVersion ver2 = ParseSemver(v2);

        // Compare major.minor.patch
        if (ver1.major != ver2.major) {
            return (ver1.major < ver2.major) ? -1 : 1;
        }
        if (ver1.minor != ver2.minor) {
            return (ver1.minor < ver2.minor) ? -1 : 1;
        }
        if (ver1.patch != ver2.patch) {
            return (ver1.patch < ver2.patch) ? -1 : 1;
        }

        // If core versions are equal, compare prereleases
        // Build metadata is ignored in precedence comparison per semver spec
        return ComparePrereleases(ver1.prerelease, ver2.prerelease);

    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid semver format");
    }
}