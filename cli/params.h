#define MINI_CASE_SENSITIVE
#include <map>
#include <string>
#include <functional>
#include <vector>
#include "ansi.h"
#include <mini/ini.h>

using std::cout, std::endl;

void PrintHelp();
void PrintVersion();
void PrintConfig();
void PrintMillennium();

struct ParameterProps {
    std::vector<std::string> ids;
    std::string description;
    std::function<void()> callback;
};

// valid base parameters
std::vector<ParameterProps> flagArguments = {
    { { "-c", "--config" }, "Print current config and quit", PrintConfig },
    { { "-v", "--version" }, "Prints the version", PrintVersion },
    { { "-h", "--help" }, "Print this help text and quit", PrintHelp }
};

struct HelpItemProps {
    std::string id;
    std::string description;
};

void PrintHelp() {

    unsigned short charLength = 0;
    std::vector<HelpItemProps> helpItems;

    std::cout << "Help: " << std::endl;

    for (const auto& arg : flagArguments) {
        std::string buffer = "  ";

        for (size_t i = 0; i < arg.ids.size(); i++) {
            buffer.append(arg.ids[i]);

            if (i != arg.ids.size() - 1) buffer.append(", ");     
        }
        helpItems.push_back({ buffer, arg.description });
    }

    for (const auto& item : helpItems) 
        charLength = item.id.length() > charLength ? item.id.length() : charLength;
    
    for (const auto& item : helpItems) 
        cout << BOLD << item.id << RESET << std::string((charLength + 5) - item.id.length(), ' ') << item.description << endl;    
}

void PrintVersion() {
    std::cout << "Version: " << std::endl;
}

void PrintConfig() {
    std::string configPath = "/home/shadow/.steam/steam/ext/millennium.ini";
    std::cout << "Config: " << configPath << "\n" << std::endl;

    mINI::INIFile file(configPath);
    mINI::INIStructure ini;

    file.read(ini);

    for (auto const& it : ini)
    {
        auto const& section = it.first;
        auto const& collection = it.second;

        cout << BOLD << section << RESET << "\n" << endl;

        int iteration = 1;
        for (auto const& it2 : collection)
        {
            auto const& key = it2.first;
            auto const& value = it2.second;
            cout << "  " << key << " = " << value << endl;

            if (iteration == collection.size()) cout << endl;
            iteration++;
        }
    }
}

void PrintMillennium() {
    cout << BOLD << "Millennium: " << MILLENNIUM_VERSION << RESET << endl;
}