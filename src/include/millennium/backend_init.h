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

#pragma once

#include "millennium/fwd_decl.h"
#include "millennium/life_cycle.h"
#include "millennium/logger.h"
#include "millennium/backend_mgr.h"
#include <memory>

class backend_initializer
{
  public:
    backend_initializer(std::shared_ptr<plugin_manager> settings_store, std::shared_ptr<backend_manager> manager, std::shared_ptr<backend_event_dispatcher> event_dispatcher)
        : m_settings_store(std::move(settings_store)), m_backend_manager(std::move(manager)), m_backend_event_dispatcher(std::move(event_dispatcher))
    {
    }
    ~backend_initializer()
    {
        logger.log("Successfully shut down backend_initializer...");
    }

    static std::shared_ptr<plugin_loader> get_plugin_loader_from_lua_vm(lua_State* L);
    static std::shared_ptr<plugin_loader> get_plugin_loader_from_python_vm();

    static void python_set_plugin_loader_ud(std::weak_ptr<plugin_loader> wp);
    static int lua_set_plugin_loader_ud(lua_State* L, std::weak_ptr<plugin_loader> wp);

    void python_backend_started_cb(plugin_manager::plugin_t plugin, const std::weak_ptr<plugin_loader> weak_plugin_loader);
    void lua_backend_started_cb(plugin_manager::plugin_t plugin, const std::weak_ptr<plugin_loader> weak_plugin_loader, lua_State* L);

    /** Fix an old issue previous versions of Millennium introduced */
    void compat_restore_shared_js_context();

    /**
     * @brief Start the package manager preload module.
     * The package manager module is responsible for python package management.
     * All packages are grouped and shared when needed, to prevent wasting space.
     */
    void start_package_manager(std::weak_ptr<plugin_loader> plugin_loader);

  private:
    void python_append_sys_path_modules(std::vector<std::filesystem::path> sitePackages);
    void python_add_site_packages_directory(std::filesystem::path customPath);
    void python_invoke_plugin_main_fn(PyObject* global_dict, std::string pluginName);
    void compat_setup_fake_plugin_settings();
    void set_plugin_environment_variables(PyObject* globalDictionary, const plugin_manager::plugin_t& plugin);
    void set_plugin_internal_name(PyObject* globalDictionary, const std::string& pluginName);

    std::shared_ptr<plugin_manager> m_settings_store;
    std::shared_ptr<backend_manager> m_backend_manager;
    std::shared_ptr<backend_event_dispatcher> m_backend_event_dispatcher;
};
