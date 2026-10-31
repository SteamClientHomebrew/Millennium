#include "millennium/encoding.h"
#include <iostream>
#include <locale>
#include <regex>

class grouped_numbers : public std::numpunct<char>
{
    std::string do_grouping() const override { return "\3"; }
    char do_thousands_sep() const override { return ','; }
};

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::locale previous = std::locale();
    if (std::string(argv[1]) == "grouped") {
        std::locale::global(std::locale(std::locale::classic(), new grouped_numbers));
    }
    const std::regex uuid_shape("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}");
    for (int i = 0; i < 128; ++i) {
        const auto uuid = GenerateUUID();
        if (!std::regex_match(uuid, uuid_shape)) {
            std::cerr << "Invalid UUID: " << uuid << '\n';
            std::locale::global(previous);
            return 1;
        }
    }
    std::locale::global(previous);
    return 0;
}
