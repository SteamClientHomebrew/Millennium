#include <string>
#include <sstream>
#include <iostream>
#include <string>
#include <iomanip>

static std::string UrlEncode(const std::string &value) 
{
    std::ostringstream encoded;
    for (unsigned char c : value) 
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') 
        {
            // Unreserved characters are not encoded
            encoded << c;
        } 
        else if (c == ' ') 
        {
            // Encode spaces as '+'
            encoded << '+';
        } 
        else 
        {
            // Encode all other characters as %XX
            encoded << '%' << std::uppercase << std::hex << int(c);
        }
    }
    return encoded.str();
}

static const std::string UrlDecode(const std::string &url) 
{
    std::string decoded;
    char ch;
    int hexValue;
    for (size_t i = 0; i < url.length(); ++i) 
    {
        if (url[i] == '%') 
        {
            // Convert the next two characters from hex to char
            if (i + 2 < url.length() && std::isxdigit(url[i + 1]) && std::isxdigit(url[i + 2])) 
            {
                std::stringstream ss;
                ss << std::hex << url.substr(i + 1, 2);
                ss >> hexValue;
                ch = static_cast<char>(hexValue);
                decoded += ch;
                i += 2; // Skip the next two characters
            }
        } 
        else if (url[i] == '+') 
        {
            // Replace '+' with space
            decoded += ' ';
        } 
        else 
        {
            // Append the character as is
            decoded += url[i];
        }
    }
    return decoded;
}


static const std::string UrlFromPath(const std::string baseAddress, const std::string path) {

    std::string outputUrl;

    #ifdef __linux__
    {
        outputUrl = baseAddress + UrlEncode(path.substr(1));
    }
    #elif _WIN32
    {
        outputUrl = baseAddress + UrlEncode(path);
    }
    #endif

    std::cout << outputUrl << std::endl;
    return outputUrl;
}

static const std::string PathFromUrl(const std::string& path)
{

    std::cout << "RECEIVED PATH: " << path << std::endl;

    #ifdef __linux__
    {
        return "/" + UrlDecode(path);
    }
    #elif _WIN32
    {
        return UrlDecode(path);
    }
    #endif
}