#include "injector.hpp"
#include "conditions/conditionals.hpp"
#include "event_handler.hpp"
#include <utils/cout/logger.hpp>

struct statementReturn
{
    bool fail;
    steam_cef_manager::script_type scriptType = steam_cef_manager::script_type::javascript;
    const std::string itemRefrence = std::string();
};

struct evalStatement
{
    nlohmann::basic_json<>& statement, selectedItem;
    std::string settingsName;
    std::vector<statementReturn>& returnVal;
};

inline const void evauluateStatement(evalStatement ev) {

    //std::cout << ev.statement.dump(4) << std::endl;

    if (ev.statement.contains("Equals") && ev.selectedItem == ev.statement["Equals"]) {
        console.log(std::format("Configuration key: [{}] Equals [{}]. Executing 'True' statement", ev.settingsName, ev.statement["Equals"].dump()));

        if (ev.statement.contains("True")) {
            const auto& trueStatement = ev.statement["True"];
            if (trueStatement.contains("TargetCss")) {
                console.log("inserting CSS module: " + trueStatement["TargetCss"].get<std::string>());
                ev.returnVal.push_back({ false, steam_cef_manager::script_type::stylesheet, trueStatement["TargetCss"] });
            }
            if (trueStatement.contains("TargetJs")) {
                console.log("inserting JS module: " + trueStatement["TargetJs"].get<std::string>());
                ev.returnVal.push_back({ false, steam_cef_manager::script_type::javascript, trueStatement["TargetJs"] });
            }
        }
        else {
            console.warn("Can't execute 'True' statement as it doesn't exist.");
        }
    }
    else {
        console.log(std::format("Configuration key: [{}] does NOT Equal [{}]. Executing 'False' statement", ev.settingsName, ev.statement["Equals"].dump()));

        if (ev.statement.contains("False")) {
            const auto& falseStatement = ev.statement["False"];
            if (falseStatement.contains("TargetCss")) {
                console.log("inserting CSS module: " + falseStatement["TargetCss"].get<std::string>());
                ev.returnVal.push_back({ false, steam_cef_manager::script_type::stylesheet, falseStatement["TargetCss"] });
            }
            if (falseStatement.contains("TargetJs")) {
                console.log("inserting JS module: " + falseStatement["TargetJs"].get<std::string>());
                ev.returnVal.push_back({ false, steam_cef_manager::script_type::javascript, falseStatement["TargetJs"] });
            }
        }
        else {
            console.warn("Can't execute 'False' statement as it doesn't exist.");
        }
    }
}

inline const std::vector<statementReturn> check_statement(const nlohmann::basic_json<>& patch)
{
    std::vector<statementReturn> returnVal;

    if (!patch.contains("Statement")) {
        return { {true} };
    }

    // parse if statement is array object or single
    std::vector<nlohmann::basic_json<>> statements;

    if (patch["Statement"].is_object())
        statements.push_back(patch["Statement"]);
    else if (patch["Statement"].is_array()) {
        for (auto& statement : patch["Statement"]) {
            statements.push_back(statement);
        }
    }

    for (auto& statement : statements) {

        //std::cout << statement.dump(4) << std::endl;

        if (!statement.contains("If")) {
            console.err("Invalid statement structure: 'If' is missing.");
            return { {true} };
        }
        if (!statement.contains("Equals") && !statement.contains("Combo")) {
            console.err("Invalid statement structure: 'Equals' or 'Combo' is missing.");
            return { {true} };
        }
        if (!skin_json_config.contains("Configuration")) {
            console.err("Invalid statement structure: 'Configuration' is missing from JSON storage.");
            return { {true} };
        }

        const auto settingsName = statement["If"].get<std::string>();
        bool isFound = false;

        for (const auto& item : skin_json_config["Configuration"]) {
            if (item.contains("Name") && item["Name"] == settingsName) {
                isFound = true;
                const auto& selectedItem = item["Value"];

                if (statement.contains("Combo")) {
                    for (auto& item : statement["Combo"]) {
                        evauluateStatement({
                            item, selectedItem, settingsName, returnVal
                            });
                    }
                }

                evauluateStatement({
                    statement, selectedItem, settingsName, returnVal
                    });
            }
        }

        if (!isFound) {
            console.err("Couldn't find config key with Name = " + settingsName);
        }
    }

    return returnVal.empty() ? std::vector<statementReturn>{ {true} } : returnVal;
}

std::vector<item> injector::find_patches(const nlohmann::json& patch, std::string cefctx_title)
{
    const auto result = check_statement(patch);

    std::vector<item> itemQuery;

    if (!result[0].fail) {
        for (const auto statementItem : result) {
            itemQuery.push_back({ statementItem.itemRefrence, statementItem.scriptType });
        }
    }

    try {
        auto conditional = conditionals::has_patch(skin_json_config, skin_json_config["native-name"], cefctx_title);

        for (auto condition : conditional)
        {
            itemQuery.push_back({ condition.item_src, condition.type });
        }
    }
    catch (const nlohmann::detail::exception& ex)
    {
        console.err(std::format("Error getting conditionals {}", ex.what()));
    }

    if (patch.contains("TargetJsList"))
    {
        for (const auto item : patch["TargetJsList"])
        {
            itemQuery.push_back({ item, steam_interface.script_type::javascript });
        }
    }
    if (patch.contains("TargetCssList"))
    {
        for (const auto item : patch["TargetCssList"])
        {
            itemQuery.push_back({ item, steam_interface.script_type::stylesheet });
        }
    }

    if (patch.contains("TargetJs"))
        itemQuery.push_back({ patch["TargetJs"].get<std::string>(), steam_interface.script_type::javascript });
    if (patch.contains("TargetCss"))
        itemQuery.push_back({ patch["TargetCss"].get<std::string>(), steam_interface.script_type::stylesheet });

    return itemQuery;
}
