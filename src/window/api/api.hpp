#pragma once

#include <string>
#include <vector>

#include <window/core/dx9_image.hpp>

class MillenniumAPI
{
public:
	struct resultsSchema
	{
		std::string discord_name, discord_link;

		std::string name;
		std::string description;
		image::image_t image_list{};

		std::string date_added;

		std::vector<std::string> v_images;

		int download_count;
		std::string file_name;

		std::string gh_username;
		std::string gh_repo;
		std::string skin_json;

		std::string id;
		std::vector<std::string> tags;
	};

	bool isDown = false;

	std::string_view endpoint = "https://millennium.web.app/api/v1";
	std::string_view endpointV2 = "https://millennium.web.app/api/v2";

	//create a get request to populate the featured array
	void retrieve_featured(void);
	void retrieve_search(const char* search_request);

	//actually get the featured array
	std::vector<resultsSchema> get_query();

	bool iterate_download_count(std::string identifier);
};

static MillenniumAPI* api = new MillenniumAPI;