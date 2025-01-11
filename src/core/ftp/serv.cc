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