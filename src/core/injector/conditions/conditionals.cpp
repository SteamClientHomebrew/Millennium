#include "conditionals.hpp"
#include <stdafx.h>
#include <iostream>
#include <set>

#include <filesystem>
#include <regex>

#include <utils/config/config.hpp>
#include <core/injector/event_handler.hpp>

const static std::filesystem::path fileName = "./.millennium/config/conditionals.json";

void save_config(std::string theme, std::string option, std::string value) 
{
	try {
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}

	nlohmann::basic_json<> conditionals;

	if (std::filesystem::exists(fileName)) 
	{
		std::ifstream ifs(fileName);
		std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

		if (!nlohmann::json::accept(content)) {
			std::cout << __func__ << " invalid json in -> " + fileName.string() << std::endl;
			return;
		}

		conditionals = nlohmann::json::parse(content);
	}

	conditionals[theme][option] = value;

	std::ofstream file(fileName);
	file << conditionals.dump(4);
}

void conditionals::setup(const nlohmann::basic_json<>& data, const std::string name)
{
	auto local = conditionals::get_conditionals(name);

	if (!data.contains("Conditions"))
	{
		return;
	}

	for (auto& [key, option] : data["Conditions"].items()) {

		if (!option.contains("values")) {
			continue;
		}

		const auto find_value = ([&](std::string name)
		{
			for (auto& [option_name, _] : option["values"].items())
			{
				if (option_name == name)
					return true;
			}
			return false;
		});

		auto first_elem = option["values"].crbegin();

		if (local.contains(key) && !find_value(local[key]))
		{
			if (option.contains("default") && find_value(option["default"]))
			{
				save_config(name, key, option["default"]);
			}
			else
			{
				std::cout << "'default' key wasn't found in values, or it doesn't exist..." << std::endl;
				save_config(name, key, first_elem.key());
			}
		}

		if (!local.contains(key))
		{
			if (option.contains("default") && find_value(option["default"]))
			{
				save_config(name, key, option["default"]);
			}
			else
			{
				// fall back
				std::cout << "'default' key wasn't found in values, or it doesn't exist..." << std::endl;
				save_config(name, key, first_elem.key());
			}
		}
	}
}

bool conditionals::update(const std::string name, std::string setting, std::string _new)
{
	std::cout << "calling " << __func__ << " folder name " << name << std::endl;

	nlohmann::basic_json<> conditionals;

	if (!std::filesystem::exists(fileName)) {
		return false;
	}

	std::ifstream ifs(fileName);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

	if (!nlohmann::json::accept(content)) {
		std::cout << __func__ << " invalid json in -> " + fileName.string() << std::endl;
		return false;
	}

	conditionals = nlohmann::json::parse(content);
	
	if (!conditionals.contains(name))
	{
		return false;
	}

	conditionals[name][setting] = _new;

	std::ofstream file(fileName);
	file << conditionals.dump(4);

	//skin_json_config = config.getThemeData();

	//steam_js_context js_context;
	//js_context.reload();
}

nlohmann::basic_json<> conditionals::get_conditionals(const std::string themeName)
{
	if (!std::filesystem::exists(fileName)) {
		std::cout << fileName << "does not exist..." << std::endl;
		return nlohmann::basic_json<>();
	}

	std::ifstream ifs(fileName);
	std::string content((std::istreambuf_iterator<char>(ifs)),(std::istreambuf_iterator<char>()));

	if (!nlohmann::json::accept(content)) {
		std::cout << __func__ << " invalid json in -> " + fileName.string() << std::endl;
		return nlohmann::basic_json<>();
	}

	auto json = nlohmann::json::parse(content);

	if (json.contains(themeName))
	{
		return json[themeName];
	}

	return nlohmann::basic_json<>();
}

std::vector<conditionals::conditional_item> 
conditionals::has_patch(const nlohmann::basic_json<>& data, const std::string name, std::string windowName)
{
	const auto check_match = [&](std::string window, std::string regex)
	{
		bool regex_match = false;

		try {
			regex_match = std::regex_search(window, std::regex(regex));
		}
		catch (std::regex_error& e) {
			console.err(std::format("Invalid regex selector: '{}' is invalid {}", regex, e.what()));
		}

		return regex_match;
	};

	auto local = conditionals::get_conditionals(name);

	std::vector<conditionals::conditional_item> result;

	const auto evaluate_type = [&](const nlohmann::basic_json<>& cond, std::string type, steam_cef_manager::script_type script_type)
	{
		if (!cond.contains(type))
		{
			return;
		}
		
		if (cond[type].contains("affects") && cond[type]["affects"].is_array())
		{
			for (const auto& window_regex : cond[type]["affects"])
			{
				if (check_match(windowName, window_regex) && cond[type].contains("src"))
				{
					result.push_back({ script_type, cond[type]["src"] });
				}
			}
		}
	};

	if (!data.contains("Conditions"))
	{
		return result;
	}

 	for (auto& [condition, object] : data["Conditions"].items()) {

		if (!object.contains("values")) {
			continue;
		}

		const auto current_value = local[condition];

		for (auto& [value, data] : object["values"].items())
		{	
			if (value != current_value)
				continue;

			evaluate_type(data, "TargetCss", steam_cef_manager::script_type::stylesheet);
			evaluate_type(data, "TargetJs", steam_cef_manager::script_type::javascript);
		}
	}

	return result;
}
