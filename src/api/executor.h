#pragma once
#include <core/loader.h>
#include <core/py_controller/co_spawn.h>

PyMethodDef* GetMillenniumModule();
void SetPluginLoader(std::shared_ptr<PluginLoader> pluginLoader);