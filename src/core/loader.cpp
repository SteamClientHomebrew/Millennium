#include "loader.hpp"
#include <string>
#include <iostream>
#include <Python.h>
#include <__builtins__/executor.h>
#include <core/backend/co_spawn.hpp>
#include <core/ipc/pipe.hpp>
#include <core/impl/mutex_impl.hpp>
#include <utilities/http.h>
#include <socket/await_pipe.h>
#include <core/hooks/web_load.h>
#include <generic/base.h>

websocketpp::client<websocketpp::config::asio_client>* plugin_client;
websocketpp::connection_hdl plugin_handle;
    
const char* API = R"(
const IPC = {
    send: (messageId, contents) => {
        MILLENNIUM_IPC_SOCKET.send(JSON.stringify({ id: messageId, data: contents }));
    },
    postMessage: (messageId, contents) => {
        return new Promise((resolve, reject) => {

            const message = { id: messageId, iteration: CURRENT_IPC_CALL_COUNT++, data: contents }

            const messageHandler = function(data) {
                const json = JSON.parse(data.data)

                if (json.id != message.iteration)
                    return

                resolve(json);
                MILLENNIUM_IPC_SOCKET.removeEventListener('message', messageHandler);
            };

            MILLENNIUM_IPC_SOCKET.addEventListener('message', messageHandler);
            MILLENNIUM_IPC_SOCKET.send(JSON.stringify(message));
        });
    }
}

window.MILLENNIUM_BACKEND_IPC = IPC

const Millennium = {
    callServerMethod: (pluginName, methodName, kwargs) => {
        return new Promise((resolve, reject) => {
            const query = {
                pluginName: pluginName,
                methodName: methodName
            }

            if (kwargs)
                query.argumentList = kwargs

            IPC.postMessage(0, query).then(response => 
            {
                const json = response
                if (json?.failedRequest) {

                    const m = " Millennium.callServerMethod() exception -> " + json.failMessage
                    eval(`// millennium can't accurately pin point where this came from
// check the sources tab and find your plugins index.js, and look for a call that could error this
throw new Error(m);
`)
                    reject()
                }

                const val = json.returnValue
                if (typeof val === 'string' || val instanceof String) {
                    resolve(atob(val))
                }
                resolve(val)
            })
        })
    }, 
    AddWindowCreateHook: (callback) => {
        g_PopupManager.AddPopupCreatedCallback((e, e1, e2) => {
            callback(e, e1, e2)
        });
    },
    findElement: (privateDocument, querySelector, timeout) => {
        return new Promise((resolve, reject) => {
            const matchedElements = privateDocument.querySelectorAll(querySelector);
            if (matchedElements.length) {
                resolve(matchedElementsanyth);
            }
            let timer = null;
            const observer = new MutationObserver(
                () => {
                    const matchedElements = privateDocument.querySelectorAll(querySelector);
                    if (matchedElements.length) {
                        if (timer) clearTimeout(timer);
                        observer.disconnect();
                        resolve(matchedElements);
                    }
                });
            observer.observe(privateDocument.body, {
                childList: true,
                subtree: true
            });
            if (timeout) {
                timer = setTimeout(() => {
                    observer.disconnect();
                    reject();
                }, timeout);
            }
        });
    }
}

window.Millennium = Millennium

)";

const void add_site_packages(std::vector<std::string> spath) 
{
    PyObject *sys_module = PyImport_ImportModule("sys");
    if (sys_module) 
    {
        PyObject *sys_path = PyObject_GetAttrString(sys_module, "path");
        if (sys_path) 
        {
            // reset PATH in case user already has python installed, to prevent conlficting versions
            PyList_SetSlice(sys_path, 0, PyList_Size(sys_path), NULL);

            for (const auto& path : spath) 
            {
                PyList_Append(sys_path, PyUnicode_FromString(path.c_str()));
            }
            Py_DECREF(sys_path);
        }
        Py_DECREF(sys_module);
    }
}

void localize_python_runtime(stream_buffer::plugin_mgr::plugin_t plugin) {

    const std::string _module = plugin.backend_abs.generic_string();
    const std::string base = plugin.base_dir.generic_string();

    plugin_manager::get().create_instance(plugin.name, [=] 
    {
        PyObject* global_dict = PyModule_GetDict((PyObject*)PyImport_AddModule("__main__"));

        // bind the plugin name to the relative interpretor
        PyDict_SetItemString(global_dict, "MILLENNIUM_PLUGIN_SECRET_NAME", PyUnicode_FromString(plugin.name.c_str()));

        std::vector<std::string> sys_paths = { 
            fmt::format("{}/backend", base),
            fmt::format("{}/.python/python311.zip", millennium_modules_path),
            fmt::format("{}/.python", millennium_modules_path),
        };

        // include venv paths to interpretor
        if (plugin.pjson.contains("venv") && plugin.pjson["venv"].is_string()) {

            const auto venv = fmt::format("{}/{}", base, plugin.pjson["venv"].get<std::string>());
            sys_paths.push_back(fmt::format("{}/Lib/site-packages", venv));    
            sys_paths.push_back(fmt::format("{}/Lib/site-packages/win32", venv));    
            sys_paths.push_back(fmt::format("{}/Lib/site-packages/win32/lib", venv));    
            sys_paths.push_back(fmt::format("{}/Lib/site-packages/Pythonwin", venv));    
        }

        add_site_packages(sys_paths);

        PyObject *obj = Py_BuildValue("s", _module.c_str());
        FILE *file = _Py_fopen_obj(obj, "r+");

        if (file == NULL) {
            console.err("failed to fopen file @ {}", _module);
            return;
        }

        int result = PyRun_SimpleFile(file, _module.c_str());

        if (result != 0) {
            // An error occurred during script execution
            console.err("millennium failed to preload [{}]", plugin.name);
            return;
        }

        // run the plugins load method 
        PyRun_SimpleString("plugin = Plugin()");
        PyRun_SimpleString("plugin._load()");
    });

    //plugin_manager::get().wait_for_exit();
}

std::string get_plugin_frontend(std::string plugin_name)
{
    return fmt::format("https://steamloopback.host/{}", plugin_name);
}

const std::string make_script(std::string filename) {
    return fmt::format("if (document.querySelectorAll(`script[src='{}'][type='module']`).length) {{ }} else {{ document.head.appendChild(Object.assign(document.createElement('script'), {{ src: '{}', type: 'module', id: 'millennium-injected' }})); }}", filename, filename);
}

bool post_socket(nlohmann::json data)
{
    //std::cout << data.dump(4) << std::endl;

    if (plugin_client == nullptr) {
        console.err("not connected to steam, cant post message");
        return false;
    }

    plugin_client->send(plugin_handle, data.dump(), websocketpp::frame::opcode::text);
    return true;
}

const void load_plugin_frontends(std::string sessionId) 
{
    webkit_handler::get().setup_hook();

    post_socket({ {"id", 1}, {"method", "Page.enable"}, {"sessionId", sessionId} });

    auto plugins = stream_buffer::plugin_mgr::parse_all();
    std::string _import;
    
    for (stream_buffer::plugin_mgr::plugin_t& plugin : plugins)  {
        _import.append("\n" + make_script(get_plugin_frontend(plugin.frontend_abs)));
    }

    std::string script = R"(
        window.MILLENNIUM_IPC_SOCKET = new WebSocket('ws://localhost:12906');
        window.CURRENT_IPC_CALL_COUNT = 0
        window.SOCKET_STATE_READY = false

        console.log("millennium is loading")
        var originalConsoleLog = console.log;
        console.log = function(message) {
            originalConsoleLog.apply(console, arguments);
            if (message.includes("async WebUITransportStore")) {
                console.log = originalConsoleLog;
                console.log("ready to load")
                )" + std::string(API) + R"(

                function load() {
                    if (!SOCKET_STATE_READY) {
                        load()
                        return
                    }
                    console.log('socket state is now ready')
                    )" + _import + R"(
                }    
                load()       
            }
        };
        MILLENNIUM_IPC_SOCKET.onopen = function (event) {
            console.log('established a connection with millennium')
            SOCKET_STATE_READY = true    
        }
    )";

    post_socket({ {"id", 8567}, {"method", "Page.addScriptToEvaluateOnNewDocument"}, {"sessionId", sessionId}, {"params", {{ "source", script }}} });
    post_socket({ {"id", 69  }, {"method", "Page.reload"}, {"sessionId", sessionId} });
}

enum method {
    SHARED_JS_CONTEXT = 2,
    GET_DOC_HEADER = 69,
    TARGET_CREATED = 99
};

const void on_message(websocketpp::client<websocketpp::config::asio_client>* c, 
            websocketpp::connection_hdl hdl, 
            websocketpp::config::asio_client::message_type::ptr msg) 
{
    const auto _thread = ([](nlohmann::basic_json<> message) 
    {
        static bool found = false;
        webkit_handler::get().handle_hook(message);

        try {

            if (message["method"] == "Target.attachedToTarget")
            {
                if (!found && message["params"]["targetInfo"]["title"] == "SharedJSContext") {
                    found = true;
                    std::string sessionId = message["params"]["sessionId"].get<std::string>();

                    console.log("Connected to steam @ {}", sessionId);
                    load_plugin_frontends(sessionId);
                }
            }

            if (message["method"] == "Target.targetInfoChanged") {

                post_socket({
                    {"id", method::TARGET_CREATED},
                    {"method", "Target.attachToTarget"},
                    {"params", { {"targetId", message["params"]["targetInfo"]["targetId"]}, {"flatten", true} }}
                });
            }
        }
        catch (nlohmann::detail::exception& ex) {
            console.err(ex.what());
        }
    });

    std::thread([_thread, c, hdl, msg] {
        const auto json = nlohmann::json::parse(msg->get_payload());

        // emit the message to the handler
        javascript::emitter::instance().emit("msg", json);
        _thread(json);
        //msg->recycle(); // cleanup
    }).detach();
}

const void on_connect(websocketpp::client<websocketpp::config::asio_client>* _c, websocketpp::connection_hdl _hdl) 
{
    plugin_client = _c; plugin_handle = _hdl;

    console.log("established connection @ {:p}", static_cast<void*>(&plugin_client));

    // auto attach targets. used to find sharedjscontext
    //post_socket({ {"id", 1}, {"method", "Target.setAutoAttach"}, {"params", {{ "flatten", true }, { "waitForDebuggerOnStart", true }, { "autoAttach", true }} } });
    post_socket({ {"id", 1}, {"method", "Target.setDiscoverTargets"}, {"params", {{ "discover", true }} } });
}

void plugin::bootstrap()
{
    console.log("bootstrapping millennium...");
    pipe::_create();

    auto plugins = stream_buffer::plugin_mgr::parse_all();

    for (const auto& plugin : plugins) {
        std::thread(localize_python_runtime, plugin).detach();
    }

    const std::string ctx = get_steam_context();

    try {
        websocketpp::client<websocketpp::config::asio_client> cl;
        cl.set_access_channels(websocketpp::log::alevel::none);
        cl.init_asio();

        cl.set_open_handler(bind(on_connect, &cl, std::placeholders::_1));
        cl.set_message_handler(bind(on_message, &cl, std::placeholders::_1, std::placeholders::_2));

        websocketpp::lib::error_code ec;
        websocketpp::client<websocketpp::config::asio_client>::connection_ptr con = cl.get_connection(ctx, ec);

        if (ec) {
            console.err("could not create connection because: {}", ec.message());
            return;
        }

        cl.connect(con); cl.run();
    }
    catch (websocketpp::exception& ex) 
    {
        console.err("Failed to connect to steam.");
        console.err(ex.what());
    }
}
