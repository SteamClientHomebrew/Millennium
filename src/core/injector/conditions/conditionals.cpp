#include "conditionals.hpp"
#include <stdafx.h>
#include <iostream>
#include <set>
#include <filesystem>
#include <regex>
#include <utils/config/config.hpp>
#include <core/injector/event_handler.hpp>

/// <summary>
/// config file that stores all saved user conditionals
/// </summary>
#ifdef _WIN32
const static std::filesystem::path fileName = "./.millennium/config/conditionals.json";
#elif __linux__
const static std::filesystem::path fileName = fmt::format("{}/.steam/steam/.millennium/config/conditionals.json", std::getenv("HOME"));;
#endif

static nlohmann::basic_json<> conditional_data_store;

/// <summary>
/// updates a single conditional in the conditions store. 
/// </summary>
/// <param name="theme">the folder name of the theme relative to steamui/skins</param>
/// <param name="option">the option that is queried to change</param>
/// <param name="value">the new updated value</param>
void save_config(std::string theme, std::string option, std::string value) 
{
	try {
		// ensure the conditional store exists before manipulation
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e) {
		console.err(fmt::format("Error: {}", e.what()));
	}

	nlohmann::basic_json<> conditionals;

	// if the condition store already exists pull the data from it so we can append the new 
	if (std::filesystem::exists(fileName)) 
	{
		std::ifstream ifs(fileName);
		// read the file content synchronisly
		std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

		// ensure the JSON structures validity before using, otherwise the whole file is rewritten.
		// data will be lost in this instance as its hard to recover. 
		if (!nlohmann::json::accept(content)) {
			std::cout << __func__ << " invalid json in -> " + fileName.string() << std::endl;
			return;
		}

		conditionals = nlohmann::json::parse(content);
	}

	conditionals[theme][option] = value;

	// dump the conditional back into the store.
	std::ofstream file(fileName);
	file << conditionals.dump(4);
}

/// <summary>
/// occurs every time a theme loads, it sets default values in the 
/// conditional config if they aren't already set. 
/// </summary>
/// <param name="data">the skin data from its skin.json respectively</param>
/// <param name="name">the folder name of the theme in /steamui/skins</param>
void conditionals::setup(const nlohmann::basic_json<>& data, const std::string name)
{
	conditional_data_store = conditionals::get_conditionals(name);

	// only check themes that have conditional queries
	if (!data.contains("Conditions"))
	{
		return;
	}

	// loop the dictionary 
	for (auto& [key, option] : data["Conditions"].items()) {

		// ensure that the values array exists and is proper iterable type
		if (!option.contains("values") && option["values"].is_array()) {
			continue;
		}

		// query a name for matching inside values array
		const auto find_value = ([&](std::string name)
		{
			// iterate over every key in the option which points to its name
			for (auto& [option_name, _] : option["values"].items())
			{
				if (option_name == name)
					return true;
			}
			return false;
		});

		// const reverse iterator to the last element in the values array.
		// sometimes doesn't work as intended and returns values 
		// synonymous to begin(), but its only for fallback purposes
		auto first_elem = option["values"].crbegin();

		// fix invalid conditionals if they aren't found in the values array.
		// ex the theme gets an update and the current selection goes out of scope
		if (conditional_data_store.contains(key) && !find_value(conditional_data_store[key]))
		{
			if (option.contains("default") && find_value(option["default"]))
			{
				// set the default storage key if it was specified in the condition
				save_config(name, key, option["default"]);
			}
			else
			{
				// fall back and use the first item in the values 
				// list if 'default' parameter points to nothing or doesn't exist
				std::cout << "'default' key wasn't found in values, or it doesn't exist..." << std::endl;
				save_config(name, key, first_elem.key());
			}
		}


		// setup the conditional defaults for a theme if they dont exist in the store. 
		if (!conditional_data_store.contains(key))
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

/// <summary>
/// called from the window handler / when a user clicks save in the editor
/// </summary>
/// <param name="name">the name of the themes parent folder</param>
/// <param name="setting">the setting trying to be updated</param>
/// <param name="_new">the new value, bool -> "yes" || "no"</param>
/// <returns></returns>
bool conditionals::update(const std::string name, std::string setting, std::string _new)
{
	//std::cout << "calling " << __func__ << " folder name " << name << std::endl;

	nlohmann::basic_json<> conditionals;

	// ensure the conditional store exists before trying to update it.
	if (!std::filesystem::exists(fileName)) {
		return false;
	}

	// read the file content from the conditionals
	std::ifstream ifs(fileName);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

	// verify the validity of the JSON structure
	if (!nlohmann::json::accept(content)) {
		std::cout << __func__ << " invalid json in -> " + fileName.string() << std::endl;
		return false;
	}

	conditionals = nlohmann::json::parse(content);
	
	// ensure the queried change has a storage specifier.
	if (!conditionals.contains(name))
	{
		return false;
	}

	conditionals[name][setting] = _new;

	// update the conditional store
	std::ofstream file(fileName);
	file << conditionals.dump(4);
	return true;
}

/// <summary>
/// returns the JSON structure of a specified theme
/// </summary>
/// <param name="themeName">the folder name i.e native-name of the theme</param>
/// <returns>the JSON structure of a specified theme</returns>
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
		// null fallback to empty structure
		return nlohmann::basic_json<>();
	}

	auto json = nlohmann::json::parse(content);

	if (json.contains(themeName))
	{
		return json[themeName];
	}

	// fallback if themename is not found in the JSON
	return nlohmann::basic_json<>();
}

/// <summary>
/// checks a specific theme if any conditionals have matches
/// and returns the conditional item structure through a vector
/// that is handled in 'event_handler.cpp'
/// </summary>
/// <param name="data">the json of the given theme</param>
/// <param name="name">the folder name / native-name of the theme</param>
/// <param name="windowName">the window/class we want to express</param>
/// <returns>std::vector<conditionals::conditional_item></returns>
std::vector<conditionals::conditional_item> 
conditionals::has_patch(const nlohmann::basic_json<>& data, const std::string name, std::string windowName)
{
	auto start_time = std::chrono::high_resolution_clock::now();

	// local lambda function simply for resuability applied in the same scope
	const auto check_match = [&](std::string window, std::string regex)
	{
		bool regex_match = false;

		try {
			// check if the window the was given matches the selector given in the skin.json conditional
			regex_match = std::regex_search(window, std::regex(regex));
		}
		catch (std::regex_error& e) {
			console.err(fmt::format("Invalid regex selector: '{}' is invalid {}", regex, e.what()));
		}

		// return the match state. 
		return regex_match;
	};


	std::vector<conditionals::conditional_item> result;

	const auto evaluate_type = [&](const nlohmann::basic_json<>& cond, std::string type, steam_cef_manager::script_type script_type)
	{
		// check if the queried conditionals contains either css or js
		if (!cond.contains(type))
		{
			// abort if it doesn't
			return;
		}
		
		// ensure the css or js has an effect on a window and if not skip it. 
		if (cond[type].contains("affects") && cond[type]["affects"].is_array())
		{
			// itrate through the items in the affects array and match them against the current window 
			for (const auto& window_regex : cond[type]["affects"])
			{
				if (check_match(windowName, window_regex) && cond[type].contains("src"))
				{
					// push back into the result schema that is evaluated in the event_handler.cpp
					result.push_back({ script_type, cond[type]["src"] });
				}
			}
		}
	};

	// ensure the theme has conditions
	if (!data.contains("Conditions"))
	{
		return result;
	}

	// loop over the conditional object / dictionary
 	for (auto& [condition, object] : data["Conditions"].items()) {

		// check if it has values 
		if (!object.contains("values")) {
			continue;
		}

		const std::string current_value = conditional_data_store[condition];

		for (auto& [value, data] : object["values"].items())
		{	
			if (value != current_value)
				continue;

			if (data.contains("vars")) 
			{
				std::string root = ":root {";
				for (auto& [var_name, var_data] : data["vars"].items()) 
				{
                    std::string root_name = var_name, variable = var_data;
					root += fmt::format("\n\t{}: {};", root_name, variable);
				}
				root += "\n}";

				std::string css = R"(	 
					document.head.appendChild(Object.assign(document.createElement("style"), { textContent: `)" + root + R"(` })); 
				)";

				result.push_back({
					steam_cef_manager::script_type::stylesheet,
					css,
					true //inline script
				});
			}

			// try to evaluate the target css and the target js against the active window
			evaluate_type(data, "TargetCss", steam_cef_manager::script_type::stylesheet);
			evaluate_type(data, "TargetJs", steam_cef_manager::script_type::javascript);
		}
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	//console.log(fmt::format("[{}][ordinal] -> {} elapsed ms", __func__, duration.count()));

	return result;
}
