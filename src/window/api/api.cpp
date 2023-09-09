#define _WINSOCKAPI_   

#include <stdafx.h>
#include <window/api/api.hpp>

std::vector<MillenniumAPI::resultsSchema> v_result_schema;

std::vector<MillenniumAPI::resultsSchema> MillenniumAPI::get_query() { return { v_result_schema }; };

void populate_array(nlohmann::basic_json<>& obj)
{
	if (obj.empty())
		return;

	//clear the array and empty the images and other information
	for (auto& item : v_result_schema) if (item.image_list.texture) item.image_list.texture->Release(); v_result_schema.clear();

	v_result_schema.resize((int)obj.size(), MillenniumAPI::resultsSchema());

	for (int i = 0; i < (int)obj.size(); i++)
	{
		std::make_shared<std::thread>([=]()
		{
			const nlohmann::basic_json<> d_info = obj[i]["download"];

			//create the header image and render it in low quality to save memory 
			const auto buffer = image::make_shared_image(obj[i]["image"][0].get<std::string>().c_str(), image::quality::low);

			v_result_schema[i] =
			{
				obj[i]["discord-server-name"], obj[i]["discord-server-link"],
				//name description header image, and the image list to show on details page
				obj[i]["name"], obj[i]["description"], { buffer.texture, buffer.width, buffer.height }, obj[i]["date-added"], obj[i]["image"].get<std::vector<std::string>>(),
				//details for adding the skin to library
				d_info["download-count"], d_info["file-name"], d_info["gh_username"], d_info["gh_repo"], d_info["skin-json"],
				//push back the theme id 
				obj[i]["_id"],
				obj[i]["tags"].get<std::vector<std::string>>()
			};
		})->detach();
	}
}

void MillenniumAPI::retrieve_featured(void)
{
	std::make_shared<std::thread>([=]()
	{
		try
		{
			nlohmann::basic_json<> response = nlohmann::json::parse(http::get(std::format("{}/featured", endpoint.data())));

			populate_array(response);
		}
		catch (const http_error& ex)
		{
			switch (static_cast<http_error::errors>(ex.code()))
			{
				case http_error::errors::couldnt_connect:
				{
					console.err("couldn't get the featured store items no internet/couldn't connect");
					return;
				}
				case http_error::errors::not_allowed:
				{
					console.err("couldn't get the featured store items. un-allowed.");
					return;
				}
			}
		}
	})->detach();
}

void MillenniumAPI::retrieve_search(const char* search_request)
{
	std::make_shared<std::thread>([=]()
	{	
		try
		{
			nlohmann::basic_json<> response = nlohmann::json::parse(http::get(std::format("{}/search/{}", endpoint.data(), search_request)));

			populate_array(response);
		}
		catch (const http_error& ex)
		{
			switch (static_cast<http_error::errors>(ex.code()))
			{
				case http_error::errors::couldnt_connect: 
				{
					console.err("couldn't get the featured store items no internet/couldn't connect");
					return;
				}
				case http_error::errors::not_allowed: 
				{
					console.err("couldn't get the featured store items. un-allowed.");
					return;
				}
			}
		}
	})->detach();
}

bool MillenniumAPI::iterate_download_count(std::string identifier)
{
	std::string url = std::format("{}/download/{}", endpoint.data(), identifier);

	console.log(url);

	const auto response = nlohmann::json::parse(http::get(url));

	return response["success"].get<bool>();
}