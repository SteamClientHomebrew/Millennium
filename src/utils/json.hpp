#pragma once

class json_str : public nlohmann::json {
public:
    // Overloaded constructor that parses the string internally
    inline json_str(const std::string& str) : nlohmann::json(nlohmann::json::parse(str)) {}
};

class json_patch {
public:
    std::string matchRegexString;
    std::string targetCss;
    std::string targetJs;
};

inline void from_json(const nlohmann::json& json, json_patch& patch) 
{
    patch = {
        json.value("MatchRegexString", std::string()),
        json.value("TargetCss", std::string()),
        json.value("TargetJs", std::string())
    };
}