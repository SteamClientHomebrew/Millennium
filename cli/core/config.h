#pragma once
#define MINI_CASE_SENSITIVE
#include <iostream>
#include <vector>
#include <util/ansi.h>
#include <mini/ini.h>
#include <logger/log.h>
#include <sys/locals.h>

std::string configPath = (SystemIO::GetSteamPath() / "ext" / "millennium.ini").generic_string();

static std::tuple<mINI::INIFile, mINI::INIStructure> GetConfigFile() {
    mINI::INIFile file(configPath);
    mINI::INIStructure ini;
    file.read(ini);

    return { file, ini };
}

void GetConfig(std::string field) {
    auto [file, ini] = GetConfigFile();
    const bool bFindField = !field.empty();

    for (auto const& it : ini) {
        const std::string& section = it.first;
        const mINI::INIMap<std::string>& collection = it.second;

        if (!bFindField) std::cout << BOLD << section << RESET << "\n" << std::endl;

        int iteration = 1;
        for (auto const& section_entry : collection) {
            auto const& key = section_entry.first;
            auto const& value = section_entry.second;

            if (!bFindField) std::cout << "  " << key << " = " << value << std::endl;
            if (key == field) std::cout << value << std::endl;
            if (!bFindField && iteration == collection.size()) std::cout << std::endl;

            iteration++;
        }
    }
}

void SetConfig(std::string field, std::string new_value) {
    auto [file, ini] = GetConfigFile();
    for (auto const& it : ini) {

        auto const& section = it.first;
        for (auto const& section_entry : it.second) {
            if (section_entry.first != field) {
                continue;
            }

            const bool bIsBool = ini[section][section_entry.first] == "yes" || 
                        ini[section][section_entry.first] == "no";

            if (bIsBool && new_value != "yes" && new_value != "no") {
                LOG_FAIL("invalid simplified boolean type for \"" << field << "\", must be [yes|no] ");
                return;
            }

            ini[section][section_entry.first] = new_value;
            file.write(ini);
            LOG_INFO("updated " << field << " to " << new_value);
        }
    }
}