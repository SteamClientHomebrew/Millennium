#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <logger/log.h>
#include <sys/locals.h>
#include <regex>
#include <typeinfo>
#include <memory>
#include <cxxabi.h>

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string CleanKey(const std::string& key) {
    std::string cleanedKey = std::regex_replace(key, std::regex("[^a-zA-Z0-9 ]"), "");
    std::replace(cleanedKey.begin(), cleanedKey.end(), ' ', '_');
    std::transform(cleanedKey.begin(), cleanedKey.end(), cleanedKey.begin(), ::tolower);
    return cleanedKey;
}

template <typename T>
std::string getSimplifiedTypeName() {
    int status = 0;
    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
        std::free
    };
    return std::is_same<T, std::string>::value ? "string" : (status == 0) ? res.get() : typeid(T).name();
}

template <typename T>
void SetValueFromFlattenedKey(nlohmann::json& j, const std::string& flattenedKey, T newValue) {
    std::vector<std::string> keys = split(flattenedKey, '.');
    nlohmann::json* current = &j;

    for (const std::string& key : keys) {
        for (auto it = current->begin(); it != current->end(); ++it) {
            std::string cleanedKey = CleanKey(it.key());
            if (cleanedKey != key) {
                continue;
            }

            if (it->is_structured()) current = &(*it);
            else 
            {
                if ((it->is_boolean() && std::is_same<T, bool>::value) || (it->is_string() && std::is_same<T, std::string>::value) ||
                    (it->is_number_integer() && std::is_same<T, int>::value) || (it->is_number_float() && std::is_same<T, double>::value)) {
                    *it = newValue;
                } else {
                    throw std::runtime_error("Tried to illegally type override a " + std::string(it->type_name()) + " with " + getSimplifiedTypeName<T>() + 
                    "\nEnsure your value type matches the existing type in the settings structure");
                }
            }
            break;
        }
    }
}

void FlattenStructure(const nlohmann::json& j, const std::string targetKey = {}, const std::string& prefix = "") {
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string cleanedKey = CleanKey(it.key());

        if (it->is_structured()) {
            FlattenStructure(*it, targetKey, prefix + cleanedKey + ".");
        } 
        else {    
            std::string typePrefix;
            if (it->is_string())              typePrefix = GREEN;
            else if (it->is_boolean())        typePrefix = BLUE;
            else if (it->is_number_integer()) typePrefix = BLUE;
            else if (it->is_number_float())   typePrefix = BLUE;
            else                              typePrefix = WHITE;

            if (!targetKey.empty() && prefix + cleanedKey != targetKey) continue;
            std::cout << BOLD << prefix + cleanedKey << RESET << " = " << typePrefix << it.value() << RESET << std::endl;
        }
    }
}

void GetThemeConfig(std::string key) {
    std::string themeConfigPath = (SystemIO::GetSteamPath() / "ext" / "themes.json").generic_string();
    nlohmann::json themeConfig = SystemIO::ReadJsonSync(themeConfigPath);

    if (key.empty()) {
        FlattenStructure(themeConfig);
        std::cout << std::endl;
        LOG_INFO("Any identifier starting with " << BOLD << BLUE "\"conditions\"" << RESET << " is part of a themes settings.");
        LOG_INFO("For any keys within " << BOLD << BLUE "\"conditions\"" << RESET << " ensure to use simplified boolean types [yes|no] instead of [true|false].");
        return;
    }
    else {
        FlattenStructure(themeConfig, key);
    }
}

template <typename T>
void SetThemeConfig(std::string key, T value) {

    std::string themeConfigPath = (SystemIO::GetSteamPath() / "ext" / "themes.json").generic_string();
    nlohmann::json themeConfig = SystemIO::ReadJsonSync(themeConfigPath);
    SetValueFromFlattenedKey(themeConfig, key, value);
    FlattenStructure(themeConfig, key);

    SystemIO::WriteFileSync(themeConfigPath, themeConfig.dump(4));
}