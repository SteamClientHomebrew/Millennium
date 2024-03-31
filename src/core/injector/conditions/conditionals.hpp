#pragma once
#include <nlohmann/json.hpp>
#include <core/steam/cef_manager.hpp>

namespace conditionals 
{
	/// <summary>
	/// setup a new theme, documentation in the source file.
	/// </summary>
	/// <param name="data"></param>
	/// <param name="folderName"></param>
	void setup(const nlohmann::basic_json<>& data, const std::string folderName);

	/// <summary>
	/// Update a conditional setting to a new value
	/// </summary>
	/// <param name="folderName">the current theme name</param>
	/// <param name="setting">the setting getting changed</param>
	/// <param name="_new">new value true == "yes" && false == "no"</param>
	bool update(const std::string folderName, std::string setting, std::string _new);

	nlohmann::basic_json<> get_conditionals(const std::string themeName);

	/// <summary>
	/// conditional result schema refrenced solely in 'has_patch' and its precursors
	/// </summary>
	struct conditional_item
	{
		steam_cef_manager::script_type type;
		std::string item_src;
		bool m_inline = false;
	};

	std::vector<conditional_item> has_wnd_patch(const nlohmann::basic_json<>& data, const std::string name, std::string windowName);
	std::vector<conditional_item> has_class_patch(const nlohmann::basic_json<>& data, const std::string classList, std::string query);
}