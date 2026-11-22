#include "millennium/encoding.h"
#include <catch2/catch_test_macros.hpp>
#include <locale>
#include <string>
#include <vector>

namespace
{
class grouped_numbers : public std::numpunct<char>
{
    std::string do_grouping() const override { return "\3"; }
    char do_thousands_sep() const override { return ','; }
};

struct locale_restore
{
    std::locale previous;
    explicit locale_restore(const std::locale& replacement) : previous(std::locale::global(replacement)) {}
    ~locale_restore() { std::locale::global(previous); }
};

bool is_hex_digit(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

bool is_uuid_v4(const std::string& value)
{
    if (value.size() != 36) return false;
    for (size_t index = 0; index < value.size(); ++index) {
        const bool separator = index == 8 || index == 13 || index == 18 || index == 23;
        if (separator ? value[index] != '-' : !is_hex_digit(value[index])) return false;
    }
    return value[14] == '4' && (value[19] == '8' || value[19] == '9' || value[19] == 'a' || value[19] == 'b');
}
}

TEST_CASE("GenerateUUID produces version 4 identifiers", "[encoding][uuid]")
{
    for (int index = 0; index < 128; ++index) {
        const std::string uuid = GenerateUUID();
        INFO("Generated UUID: " << uuid);
        CHECK(is_uuid_v4(uuid));
    }
}

TEST_CASE("GenerateUUID ignores the global locale", "[encoding][uuid]")
{
    std::vector<std::string> identifiers;
    identifiers.reserve(128);
    {
        locale_restore restore(std::locale(std::locale::classic(), new grouped_numbers));
        for (int index = 0; index < 128; ++index) identifiers.push_back(GenerateUUID());
    }

    for (const std::string& uuid : identifiers) {
        INFO("Generated UUID: " << uuid);
        CHECK(is_uuid_v4(uuid));
    }
}
