#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class http_error : public std::exception
{
public:
    enum errors {
        couldnt_connect,
        not_allowed,
        miscellaneous,
    };

    http_error(int errorCode) : errorCode_(errorCode) {}

    int code() const {
        return errorCode_;
    }

private:
    int errorCode_;
};

//no information is transfered from the client to elsewhere other than download count on skins.
//this is specifically for get requests to the steam socket and remotely hosted skins (getting skin info)
class http
{
public:
	static const std::string get(std::string remote_endpoint);
	static const nlohmann::json getJson(std::string remote_endpoint);

    static const bool to_disk(const char* url, const char* path);
    static const std::string post(std::string remote_endpoint, std::string data);
};