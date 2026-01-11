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

#include "millennium/singleton.h"
#include "millennium/backend_mgr.h"
#include "millennium/logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <Python.h>
#include <lua.hpp>

class PythonGIL : public std::enable_shared_from_this<PythonGIL>
{
  private:
    PyGILState_STATE m_interpreterGIL{};
    PyThreadState* m_interpreterThreadState = nullptr;
    PyInterpreterState* m_mainInterpreter = nullptr;
    std::unique_lock<std::mutex> m_interpreterLock;

  public:
    void HoldAndLockGIL();
    void HoldAndLockGILOnThread(std::shared_ptr<PythonThreadState> threadState);
    void ReleaseAndUnLockGIL();

    PythonGIL();
    ~PythonGIL();
};

enum FFI_Type
{
    Boolean,
    String,
    JSON,
    Integer,
    Error,
    UnknownType // non-atomic ADT's
};

static const std::unordered_map<nlohmann::json::value_t, FFI_Type> FFIMap_t = {
    { nlohmann::json::value_t::boolean,         FFI_Type::Boolean },
    { nlohmann::json::value_t::string,          FFI_Type::String  },
    { nlohmann::json::value_t::number_integer,  FFI_Type::Integer },
    { nlohmann::json::value_t::number_unsigned, FFI_Type::Integer },
    { nlohmann::json::value_t::object,          FFI_Type::JSON    },
    { nlohmann::json::value_t::array,           FFI_Type::JSON    }
};

struct EvalResult
{
    FFI_Type type;
    std::string plain;
};

namespace Lua
{
EvalResult LockAndInvokeMethod(std::string pluginName, nlohmann::json script);
void CallFrontEndLoaded(std::string pluginName);
} // namespace Lua

namespace Python
{
EvalResult LockGILAndInvokeMethod(std::string pluginName, nlohmann::json script);

std::tuple<std::string, std::string> ActiveExceptionInformation();

EvalResult LockGILAndInvokeMethod(std::string pluginName, nlohmann::json script);
void CallFrontEndLoaded(std::string pluginName);
} // namespace Python

struct JsEvalResult
{
    nlohmann::basic_json<> json;
    bool successfulCall;
};

namespace JavaScript
{

enum Types
{
    Boolean,
    String,
    Integer
};

struct JsFunctionConstructTypes
{
    std::string pluginName;
    Types type;
};

JsEvalResult ExecuteOnSharedJsContext(std::string javaScriptEval);
const std::string ConstructFunctionCall(const char* value, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> params);

int Lua_EvaluateFromSocket(std::string script, lua_State* L);
PyObject* Py_EvaluateFromSocket(std::string script);
} // namespace JavaScript