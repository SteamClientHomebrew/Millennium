class json_str : public nlohmann::json {
public:
    // Overloaded constructor that parses the string internally
    json_str(const std::string& str) : nlohmann::json(nlohmann::json::parse(str)) {}
};

class json_patch {
public:
    std::string matchRegexString;
    std::string targetCss;
    std::string targetJs;

    // Assignment operator
    //json_patch& operator=(const json_patch& other) {
    //    if (this != &other) {
    //        matchRegexString = other.matchRegexString;
    //        targetCss = other.targetCss;
    //        targetJs = other.targetJs;
    //    }
    //    return *this;
    //}

    //// Define the conversion function from_json inside the class
    //static void from_json(const nlohmann::json& j, json_patch& patch) {
    //    patch.matchRegexString = j.at("MatchRegexString").get<std::string>();

    //    if (j.contains("TargetCss"))
    //        patch.targetCss = j.at("TargetCss").get<std::string>();
    //    if (j.contains("TargetJs"))
    //        patch.targetJs = j.at("TargetJs").get<std::string>();
    //}
};