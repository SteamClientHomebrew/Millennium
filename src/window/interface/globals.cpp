#define _WINSOCKAPI_   

#include <stdafx.h>
#include "globals.h"

nlohmann::basic_json<> millennium::readFileSync(std::string path)
{
	std::ifstream skin_json_file(path);
	nlohmann::basic_json<> data;

	std::string file_content((std::istreambuf_iterator<char>(skin_json_file)), std::istreambuf_iterator<char>());
	skin_json_file.close();

	if (!nlohmann::json::accept(file_content))
	{
		std::string name = std::filesystem::path(path).parent_path().filename().string();
		std::string message = fmt::format("A theme named [{}] was detected as broken, specifically the skin.json is invalid. If you can't fix it, remove the theme from the themes folder.\n\n\nWould you like to automatically delete this theme?\nYou have to restart Steam after.", name);
		//MsgBox(message, "Error", MB_ICONERROR);

		//MsgBox("Error", [&](auto open) {

		//	ImGui::TextWrapped(message.c_str());

		//	if (ImGui::Button("Delete Theme"))
		//	{
		//		auto parent = std::filesystem::path(path).parent_path();
		//		std::filesystem::remove_all(std::filesystem::path(parent));
		//	}
		//	ImGui::SameLine();
		//	ui::shift::x(-4);
		//	if (ImGui::Button("Close")) {
		//		*open = false;
		//	}
		//});

		auto selection = msg::show(message.c_str(), "Oops!", Buttons::YesNo);

		std::cout << std::filesystem::path(path).parent_path().string() << std::endl;

		if (selection == Selection::Yes)
		{
			try {
				auto parent = std::filesystem::path(path).parent_path();
				std::filesystem::remove_all(std::filesystem::path(parent));
			}
			catch (const std::filesystem::filesystem_error& ex)
			{
				msg::show(fmt::format("couldn't delete the conflicting theme:\n\n{}", ex.what()).c_str(), "Another Oops :(");
			}
		}
		exit(0);
	}

	data = nlohmann::json::parse(file_content, nullptr, true, true);
	return data;
}

bool millennium::isInvalidSkin(std::string skin)
{
	const std::string steamPath = config.getSkinDir();

	if (!std::filesystem::exists(fmt::format("{}/{}/skin.json", steamPath, skin))
		&& !std::filesystem::exists(fmt::format("{}/{}", steamPath, skin)))
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

bool checkForUpdates(nlohmann::basic_json<>& data, std::filesystem::path skin_json_path)
{
	if (data.contains("source")) 
	{
		std::filesystem::path cloudData = std::filesystem::path(data["source"].get<std::string>()) / "skin.json";
		try 
		{
			nlohmann::basic_json<> cloudResponse = http::getJson(cloudData.string());

			if (cloudResponse.contains("download_url")) {
				nlohmann::basic_json<> jsonResponse = http::getJson(cloudResponse["download_url"]);

				const auto localVersion = data["version"].get<std::string>();
				const auto cloudVersion = jsonResponse["version"].get<std::string>();

				bool needsUpdate = localVersion != cloudVersion;

				console.imp(fmt::format("  > Installed version: {}, Cloud version: {}", localVersion, cloudVersion));
				console.log(fmt::format("  > {} -> {}", skin_json_path.string(), needsUpdate ? "needs update" : "is up-to-date"));

				return needsUpdate;
			}
			else {
				console.err("  > Invalid source key inside of skin. Can't use it to check for updates");
				return false;
			}
		}
		catch (const http_error&) {
			console.log(fmt::format("  > An HTTP error occurred while checking for updates on skin {}", skin_json_path.string()));
			return false;
		}
	}
	else {
		console.log("there is no source for this theme, can't check for updates...");
		return false;
	}
}

bool millennium::parseLocalSkin(const std::filesystem::directory_entry& entry, std::vector<nlohmann::basic_json<>>& buffer, bool _checkForUpdates)
{
	std::filesystem::path skin_json_path = entry.path() / "skin.json";

	if (!std::filesystem::exists(skin_json_path))
		return false;

	auto data = this->readFileSync(skin_json_path.string());

	const std::string fileName = entry.path().filename().string();

	data["name"]        = data.value("name", fileName).c_str();
	data["native-name"] = fileName;

	if (!data.contains("description")) {
		data["description"] = "no description yet.";
	}

	data["remote"]      = false;

	buffer.push_back(data);
	return true;
}

nlohmann::basic_json<> millennium::bufferSkinData()
{
	const std::string steamPath = config.getSkinDir();
	console.log(fmt::format("Searching for steam skins at -> [{}]", steamPath));

	this->resetCollected();
	std::vector<nlohmann::basic_json<>> jsonBuffer;

	for (const auto& entry : std::filesystem::directory_iterator(steamPath))
	{
		console.log(fmt::format("Folder -> [{}]", entry.path().filename().string()));

		if (entry.is_directory())
		{
			if (!this->parseLocalSkin(entry, jsonBuffer, true))
				continue;
		}
	}
	return jsonBuffer;
}


bool alreadyExists(const nlohmann::json& data, const std::string& targetOwner) {
	// Use std::any_of to check if any object has the specified owner
	return std::any_of(data.begin(), data.end(),
		[targetOwner](const nlohmann::json& item) {
			return item.contains("owner") && item["owner"] == targetOwner;
		});
}

std::vector<nlohmann::basic_json<>> millennium::add_update_status_to_client(
	std::vector<nlohmann::basic_json<>>& buffer, nlohmann::json nativeName, bool needsUpdate) {

	bool found = false;

	for (auto& item : buffer) {
		if (item["native-name"] == nativeName["name"]) {
			item["update_required"] = needsUpdate;
			item["git"]["url"] = nativeName["url"];
			item["git"]["date"] = nativeName["date"];
			item["git"]["commit"] = nativeName["commit"];
			item["git"]["message"] = nativeName["message"];
			item["git"]["download"] = nativeName["download"];
			found = true;
		}
	}

	return buffer;
}

static const std::filesystem::path fileName = ".millennium/manager/hashes-frontend.json";

nlohmann::json get_versions_json() {

	nlohmann::json parsedData;

	std::ifstream inputFile(fileName);

	if (inputFile.is_open()) {
		std::string fileContent((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());

		parsedData = nlohmann::json::accept(fileContent) ? nlohmann::json::parse(fileContent) : nlohmann::json();
		inputFile.close();
	}

	return parsedData;
}

nlohmann::json millennium::get_versions_from_disk(std::vector<nlohmann::basic_json<>>& buffer) {
	nlohmann::json parsedData = get_versions_json();

	for (const auto& item : buffer) {

		if (!item.contains("github") || !item["github"].is_object() ||
			!item["github"].contains("owner") || !item["github"].contains("repo_name"))
			continue;

		if (!alreadyExists(parsedData, item["github"]["owner"])) {
			parsedData.push_back({
				{"owner", item["github"]["owner"]},
				{"repo", item["github"]["repo_name"]},
				{"name", item["native-name"]}
			});
		}
		else {
			for (auto& data : parsedData) {
				if (data.value("owner", "null") == item["github"].value("owner", "_null")) {
					data["repo"] = item["github"]["repo_name"];
					data["name"] = item["native-name"];
				}
			}
		}
	}

	return parsedData;
}

std::vector<nlohmann::basic_json<>> millennium::get_update_list(
	std::vector<nlohmann::basic_json<>>& buffer,
	bool reset_version,
	std::string reset_name
) {
	auto start_time = std::chrono::high_resolution_clock::now();

	nlohmann::json parsedData = get_versions_from_disk(buffer);

	if (parsedData.is_null()) {
		return buffer;
	}

	auto cloud_versions = nlohmann::json();

	try {
		console.log("Checking themes for updates...");

		const auto response = http::post("/api/v2/checkupdates", parsedData.dump(4).c_str());

		cloud_versions = nlohmann::json::parse(response);
	}
	catch (const http_error ex) {
		console.err("No internet connection, can't check for updates on themes");
	}
	catch (nlohmann::detail::exception&) {}

	for (const auto& cloud : cloud_versions) {
		for (auto& local : parsedData) {

			if (local["repo"] != cloud["name"]) 
				continue;

			local["url"] = cloud["url"];
			local["date"] = cloud["date"];
			local["message"] = cloud["message"];
			local["download"] = cloud["download"];

			if (!local.contains("commit")) {
				console.log("Setting update hash as it was previously unset.");

				local["commit"] = cloud["commit"];
				buffer = add_update_status_to_client(buffer, local, false);
				continue;
			}
			// When the skin is done updating we set the commit hash to the latest version
			else if (reset_version) {
				if (local["name"] == reset_name) {
					local["commit"] = cloud["commit"];
				}
			}

			if (local["commit"] == cloud["commit"]) {
				console.log(fmt::format("[{}] is up-to-date", local["name"].get<std::string>()));
				buffer = add_update_status_to_client(buffer, local, false);
				local["up-to-date"] = true;

			}
			else {
				console.log("Not up-to-date, getting latest update...");
				buffer = add_update_status_to_client(buffer, local, true);
				local["up-to-date"] = false;
			}
		}
	}

	try {
		std::filesystem::create_directories(fileName.parent_path());
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << std::endl;
	}

	std::ofstream outputFile(fileName);

	if (outputFile.is_open()) {
		outputFile << std::setw(4) << parsedData;
		outputFile.close();
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

	console.log(fmt::format("Update check -> {} elapsed ms", duration.count()));

	return buffer;
}

std::vector<nlohmann::basic_json<>>
	millennium::use_local_update_cache(std::vector<nlohmann::basic_json<>>& buffer) {

	console.log("Opting to use local update cache");

	const auto& local = get_versions_from_disk(buffer);

	for (auto& item : buffer) {
		for (auto& update_listing : local) {
			if (update_listing["name"] == item["native-name"]) {
				item["update_required"] = !update_listing.value("up-to-date", true);

				item["git"]["url"]     = update_listing.value("url", "null");
				item["git"]["date"]    = update_listing.value("date", "null");
				item["git"]["commit"]  = update_listing.value("commit", "null");
				item["git"]["message"] = update_listing.value("message", "null");
				item["git"]["download"] = update_listing.value("download", "null");
			}
		}
	}

	return buffer;
}

bool compareByLastWriteTime(const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
	return 
		std::filesystem::last_write_time(a).time_since_epoch() 
		> std::filesystem::last_write_time(b).time_since_epoch();
}

void millennium::parseSkinData(bool checkForUpdates, bool setCommit, std::string newCommit)
{
	console.log(fmt::format("Calling {} with param -> {}", __func__, checkForUpdates));

	const std::string steamPath = config.getSkinDir();

	this->resetCollected();


	std::vector<std::filesystem::directory_entry> directories;

	if (!std::filesystem::exists(steamPath)) {
		//MsgBox("Couldn't find skins folder? it disappeared :O", "Error", MB_ICONERROR);


		auto selection = msg::show("Couldn't find skins folder? it disappeared :O", "Error", Buttons::OK);
		return;
	}

	// Iterate over the directories and store them in the vector
	for (const auto& entry : std::filesystem::directory_iterator(steamPath)) {
		if (entry.is_directory()) {
			directories.push_back(entry);
		}
	}

	// Sort the vector based on last write time in descending order
	std::sort(directories.begin(), directories.end(), compareByLastWriteTime);


	std::vector<nlohmann::basic_json<>> jsonBuffer;

	for (const auto& entry : directories) {
		// Process the directory entry and populate the jsonBuffer
		if (!this->parseLocalSkin(entry, jsonBuffer, false)) {
			continue;
		}
	}

	// check for updates on the current skin. 
	// disabled, to lazy to implement right now :)

	switch (checkForUpdates) {
		case true:
			try {
				jsonBuffer = get_update_list(jsonBuffer, setCommit, newCommit);
			}
			catch (const http_error ex) {
				switch (ex.code()) {
					case http_error::couldnt_connect: 
						console.err("No internet connection, can't check for updates on themes");
				}
			}
			break;
		case false:
			jsonBuffer = use_local_update_cache(jsonBuffer);
			break;
	}

	skinData = jsonBuffer;
	skinDataReady = true;
}

void millennium::changeSkin(nlohmann::basic_json<>& skin)
{
	std::string skinName = skin["native-name"].get<std::string>();

	console.log(fmt::format("updating selected skin -> {}", skinName == m_currentSkin ? "default" : skinName));

	Settings::Set("active-skin", skinName == m_currentSkin ? "default" : skinName);
	themeConfig::updateEvents::getInstance().triggerUpdate();

	steam_js_context js_context;
	js_context.reload();

	//if (steam::get().params.has("-silent")) {
	//	MsgBox("Steam is launched in -silent mode so you need to open steam again from the task tray for it to re-open", "Millennium", MB_ICONINFORMATION);
	//}

	parseSkinData(false);
}