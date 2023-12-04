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

class http
{
public:
    static const std::string get(std::string remote_endpoint);
};