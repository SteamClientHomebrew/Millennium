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
#include "locals.h"
#include "log.h"
#include "asio.h"
#include "encoding.h"
#include "url_parser.h"
#include "serv.h"

namespace Crow
{
    /**
     * A struct representing the properties of an HTTP response.
     * 
     * @typedef {Object} ResponseProps
     * @property {string} contentType - The MIME type of the response content (e.g., "text/css").
     * @property {string} content - The actual content of the response.
     * @property {boolean} exists - A flag indicating if the requested file exists or not.
     */
    struct ResponseProps
    {
        std::string contentType;
        std::string content;
        bool exists;
    };

    /**
     * Evaluates an HTTP request by determining the content type and reading the file content.
     *
     * @param {std::filesystem::path} path - The path of the requested file.
     * @returns {ResponseProps} - The properties of the response, including content type, content, and existence status.
     * 
     * This function determines the type of the requested file (using `EvaluateFileType`), then reads the content of the file 
     * and returns an appropriate response structure (`ResponseProps`).
     */
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

    /**
     * Handles an incoming HTTP request by generating a response with the appropriate content.
     *
     * @param {string} path - The request path (file path).
     * @returns {crow::response} - The response to be sent back to the client.
     * 
     * This function checks if the requested file exists, sets the correct content type, and writes the content to the response.
     * If the file does not exist, a 404 error response is returned.
     */
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

    /**
     * Binds a Crow application to a random open port and sets up routing for incoming requests.
     *
     * @returns {std::tuple} - A tuple containing the `crow::SimpleApp` instance and the assigned port number.
     * 
     * This function sets up a `crow::SimpleApp`, binds it to a random open port, and routes all incoming paths 
     * to the `HandleRequest` function for handling.
     */
    std::tuple<std::shared_ptr<crow::SimpleApp>, uint16_t> BindApplication()
    {
        uint_least16_t port = Asio::GetRandomOpenPort();
        std::shared_ptr<crow::SimpleApp> app = std::make_shared<crow::SimpleApp>();

        app->server_name(fmt::format("millennium/{}", MILLENNIUM_VERSION));
        app->loglevel(crow::LogLevel::Critical);
        app->port(port);

        /** Bind the request handler for all request paths */
        CROW_ROUTE((*app), "/<path>")(HandleRequest);
        return std::make_tuple(app, port);
    }

    /**
     * Creates and starts an asynchronous server that handles requests concurrently.
     *
     * @returns {uint16_t} - The port number on which the server is running.
     * 
     * This function initializes the Crow application, binds it to a random open port, 
     * and runs it in a separate thread. It then returns the assigned port number.
     */
    uint16_t CreateAsyncServer()
    {
        auto [app, port] = BindApplication();
        std::thread([app]() mutable { app->multithreaded().run(); }).detach();
        return port;
    }
}