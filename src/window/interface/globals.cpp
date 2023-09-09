#define _WINSOCKAPI_   

#include <stdafx.h>
#include "globals.h"

nlohmann::basic_json<> millennium::readFileSync(std::string path)
{
	std::ifstream skin_json_file(path);
	nlohmann::basic_json<> data;

	std::string file_content((std::istreambuf_iterator<char>(skin_json_file)), std::istreambuf_iterator<char>());
	data = nlohmann::json::parse(file_content, nullptr, true, true);

	return data;
}

bool millennium::isInvalidSkin(std::string skin)
{
	const std::string steamPath = config.getSkinDir();

	if (!std::filesystem::exists(std::format("{}/{}/skin.json", steamPath, skin))
		&& !std::filesystem::exists(std::format("{}/{}", steamPath, skin)))
	{
		return true;
	}
	return false;
}

void millennium::resetCollected()
{
	remoteSkinData.clear();

	m_missingSkins = 0;
	m_unallowedSkins = 0;

	if (this->isInvalidSkin())
		Settings::Set("active-skin", "default");

	m_currentSkin = Settings::Get<std::string>("active-skin");
}

void millennium::parseRemoteSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer)
{
	const auto skinStream = this->readFileSync(entry.path().string());
	remoteSkinData.push_back(skinStream);

	try
	{
		nlohmann::basic_json<> result = nlohmann::json::parse(http::get(skinStream["skin-json"]));

		const std::string fileName = entry.path().filename().string();

		//merge the local data from the skin with its remote ending
		result["github"]["username"] = skinStream["gh_username"];
		result["github"]["repo"] = skinStream["gh_repo"];

		result["name"] = result.value("name", fileName).c_str();
		result["native-name"] = fileName;
		result["description"] = result.value("description", "no description yet.").c_str();
		result["remote"] = true;

		buffer.push_back(result);
	}
	catch (const http_error& exception)
	{
		switch (exception.code())
		{
			case http_error::errors::not_allowed: m_unallowedSkins++; break;
			case http_error::errors::couldnt_connect: m_missingSkins++; break;
		}
	}
}

void millennium::parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer)
{
	std::filesystem::path skin_json_path = entry.path() / "skin.json";

	if (!std::filesystem::exists(skin_json_path))
		return;

	auto data = this->readFileSync(skin_json_path.string());

	const std::string fileName = entry.path().filename().string();

	data["name"]        = data.value("name", fileName).c_str();
	data["native-name"] = fileName;
	data["description"] = data.value("description", "no description yet.").c_str();
	data["remote"]      = false;

	buffer.push_back(data);
}

void millennium::getRawImages(std::vector<std::string>& images)
{
	for (auto& item : v_rawImageList) {
		if (item.texture) item.texture->Release();
	}

	v_rawImageList.clear();
	v_rawImageList.resize((int)images.size(), { nullptr, 0, 0 });

	// use threading to load the images on their own thread synchronously
	for (int i = 0; i < (int)images.size(); i++) {
		std::thread([=]() { v_rawImageList[i] = image::make_shared_image(images[i].c_str(), image::quality::high); }).detach();
	}
}

void millennium::parseSkinData()
{
	const std::string steamPath = config.getSkinDir();

	try
	{
		this->resetCollected();

		//create a buffer so we dont clear the buffer displaying on screen else it could cause visual bugs or crashes
		std::vector<nlohmann::basic_json<>> jsonBuffer;

		for (const auto& entry : std::filesystem::directory_iterator(steamPath))
		{
			if (entry.path().filename().string().find(".millennium.json") != std::string::npos)
			{
				this->parseRemoteSkin(entry, jsonBuffer);
			}
			else if (entry.is_directory())
			{
				this->parseLocalSkin(entry, jsonBuffer);
			}
		}
		//replace the current json with the buffer
		skinData = jsonBuffer;
	}
	catch (std::exception& ex)
	{
		console.err(std::format("Error getting user settings. Message: {}", ex.what()));
	}
}

void millennium::changeSkin(nlohmann::basic_json<>& skin)
{
	std::string skinName = skin["native-name"].get<std::string>();

	// the selected skin was clicked (i.e to deselect)
	if (m_currentSkin._Equal(skinName)) {
		skinName = "default";
	}

	//whitelist the default skin
	if (skinName == "default" || !this->isInvalidSkin(skinName))
	{
		console.log("updating selected skin.");

		Settings::Set("active-skin", skinName);
		themeConfig::updateEvents::getInstance().triggerUpdate();

		steam_js_context js_context;
		js_context.reload();

		if (steam::get().params.has("-silent")) {
			MsgBox("Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium", MB_ICONINFORMATION);
		}
	}
	else {
		MsgBox("the selected skin was not found, therefor can't be loaded.", "Millennium", MB_ICONINFORMATION);
	}

	parseSkinData();
}

void millennium::concatLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin)
{
	std::ofstream file(std::format("{}/{}", config.getSkinDir(), item.file_name));

	if (file.is_open()) {
		file << skin.dump(4); file.close();
	}
	else {
		console.err("unable to add skin to library");
	}
}

void millennium::dropLibraryItem(MillenniumAPI::resultsSchema item, nlohmann::json& skin)
{
	if (!std::filesystem::remove(std::format("{}/{}", config.getSkinDir(), item.file_name))) {
		console.err("couldn't remove skin from library");
	}
}