/**
 * ==================================================
 *   _____ _ _ _             _                     
 *  |     |_| | |___ ___ ___|_|_ _ _____           
 *  | | | | | | | -_|   |   | | | |     |          
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|          
 * 
 * ==================================================
 * 
 * Copyright (c) 2025 Project Millennium
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * serv.cc
 * 
 * This FTP is used strictly for internal use.
 * The port in which the FTP resides on is dynamic and is chosen at runtime to 
 * prevent any unwanted interaction from external sources.
 * 
 * This server is primarily used to provide a way to read local files 
 * from the host machine from the SteamUI "virtual machine".
 * ex. Loading plugin files, assets, etc.
 */

#include <iostream>
#include <crow.h>
#include <sys/locals.h>
#include <sys/log.h>
#include <sys/asio.h>
#include <sys/encoding.h>
#include <util/url_parser.h>

enum eFileType
{
    StyleSheet,
    JavaScript,
    Json,
    Python,
    Other
};

/** FTP File types, these should be enough */
static std::map<eFileType, std::string> fileTypes 
{
    { eFileType::StyleSheet, "text/css" },
    { eFileType::JavaScript, "application/javascript" },
    { eFileType::Json,       "application/json" },
    { eFileType::Python,     "text/x-python" },
    { eFileType::Other,      "text/plain" }
};

const eFileType EvaluateFileType(std::filesystem::path filePath)
{
    const std::string extension = filePath.extension().string();

    if      (extension == ".css")  { return eFileType::StyleSheet; }
    else if (extension == ".js")   { return eFileType::JavaScript; }
    else if (extension == ".json") { return eFileType::Json; }
    else if (extension == ".py")   { return eFileType::Python; }

    else
    {
        return eFileType::Other;
    }
}

namespace Crow
{
    struct ResponseProps
    {
        std::string contentType;
        std::string content;
        bool exists;
    };

    ResponseProps EvaluateRequest(std::filesystem::path path)
    {
        eFileType fileType = EvaluateFileType(path.string());
        const std::string contentType = fileTypes[fileType];

        return {
            contentType,
            SystemIO::ReadFileSync(path.string()),
            std::filesystem::exists(path)
        };
    }

    crow::response HandleRequest(std::string path)
    {
        crow::response response;
        ResponseProps responseProps = EvaluateRequest(PathFromUrl(path));

        response.add_header("Content-Type", responseProps.contentType);
        response.add_header("Access-Control-Allow-Origin", "*");

        if (!responseProps.exists)
        {
            response.code = 404;
            response.write(fmt::format("404 File not found: {}", PathFromUrl(path)));
            return response;
        }

        response.write(responseProps.content);
        return response;
    }

    std::tuple<std::shared_ptr<crow::SimpleApp>, uint16_t> BindApplication()
    {
        uint_least16_t port = Asio::GetRandomOpenPort();
        std::shared_ptr<crow::SimpleApp> app = std::make_shared<crow::SimpleApp>();

        app->server_name("millennium/1.0");
        app->loglevel(crow::LogLevel::Critical);
        app->port(port);

        /** Bind the request handler for all request paths */
        CROW_ROUTE((*app), "/<path>")(HandleRequest);
        return std::make_tuple(app, port);
    }

    uint16_t CreateAsyncServer()
    {
        auto [app, port] = BindApplication();

        std::thread(
            [app]() mutable {
                app->multithreaded().run();
            }
        ).detach();

        return port;
    }
}