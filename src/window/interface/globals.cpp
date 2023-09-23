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

bool millennium::parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer)
{
	std::filesystem::path skin_json_path = entry.path() / "skin.json";

	console.log(std::format("local skin parser: {}", skin_json_path.string()));

	if (!std::filesystem::exists(skin_json_path))
		return false;

	auto data = this->readFileSync(skin_json_path.string());

	console.log(std::format("[DEBUG] found a skin: {}", entry.path().filename().string()));
	const std::string fileName = entry.path().filename().string();

	if (data.contains("source"))
	{
		std::filesystem::path cloudData = std::filesystem::path(data["source"].get<std::string>()) / "skin.json";

		try {
			nlohmann::basic_json<> cloudResponse = nlohmann::json::parse(http::get(cloudData.string()));

			if (cloudResponse.contains("download_url")) {
				cloudResponse = nlohmann::json::parse(http::get(cloudResponse["download_url"].get<std::string>()));
			}

			std::cout << "checking current version: " << data["version"].get<std::string>() << std::endl;
			std::cout << "checking repo version: " << cloudResponse["version"].get<std::string>() << std::endl;

			if (data["version"] == cloudResponse["version"]) {
				console.log(std::format("{} is currently up-to-date", skin_json_path.string()));
				data["update_required"] = false;
			} else {
				console.log(std::format("{} needs an update", skin_json_path.string()));
				data["update_required"] = true;

				if (cloudResponse.contains("patch-notes")) 
				{		
					try
					{
						std::string patchNotes = nlohmann::json::parse(http::get(cloudResponse["patch-notes"]))["body"];

						console.log(std::format("Got patch notes for skin that needed an update:\n{}", patchNotes));

						data["patch-notes"] = patchNotes;
					}
					catch (nlohmann::detail::exception& excep) {
						console.log(std::format("Exception while getting patch notes (json parser): {}", excep.what()));
					}
					catch (const http_error& error) {
						console.log(std::format("Exception while getting patch notes (http error): {}", error.what()));
					}
				}
			}
		}
		catch (const http_error& error) {
			console.log(std::format("An error occured while checking for updates on skin {}", skin_json_path.string()));
		}
	}

	data["name"]        = data.value("name", fileName).c_str();
	data["native-name"] = fileName;
	data["description"] = data.value("description", "no description yet.").c_str();
	data["remote"]      = false;

	buffer.push_back(data);
	return true;
}

void millennium::releaseImages() {
	for (auto& item : v_rawImageList) {
		if (item.texture) item.texture->Release();
	}

	v_rawImageList.clear();
}

void millennium::getRawImages(std::vector<std::string>& images)
{
	for (auto& item : v_rawImageList) {
		std::cout << "Iterating over texture" << std::endl;
		if (item.texture)
		{
			std::cout << "Releasing a texture" << std::endl;
			item.texture->Release();
		}
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
	std::thread([&]() {
		const std::string steamPath = config.getSkinDir();
		console.log(std::format("[DEBUG] searching for steam skins at: {}", steamPath));
		try
		{
			this->resetCollected();
			std::vector<nlohmann::basic_json<>> jsonBuffer;

			for (const auto& entry : std::filesystem::directory_iterator(steamPath))
			{
				console.log("found a folder in skins folder, checking if its a valid skin.");

				if (entry.is_directory())
				{
					if (!this->parseLocalSkin(entry, jsonBuffer))
						continue;
				}
			}
			skinData = jsonBuffer;
		}
		catch (std::exception& ex) {
			console.err(std::format("Error getting user settings. Message: {}", ex.what()));
		}
	}).detach();
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