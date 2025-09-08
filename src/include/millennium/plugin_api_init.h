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
#include "co_spawn.h"
#include "ffi.h"
#include "loader.h"

static std::map<std::string, JavaScript::Types> typeMap = {
    { "string",  JavaScript::Types::String  },
    { "boolean", JavaScript::Types::Boolean },
    { "number",  JavaScript::Types::Integer }
};

static std::string getMonthNumber(const std::string& monthAbbr)
{
    static std::map<std::string, std::string> monthMap{
        { "Jan", "01" },
        { "Feb", "02" },
        { "Mar", "03" },
        { "Apr", "04" },
        { "May", "05" },
        { "Jun", "06" },
        { "Jul", "07" },
        { "Aug", "08" },
        { "Sep", "09" },
        { "Oct", "10" },
        { "Nov", "11" },
        { "Dec", "12" }
    };
    return monthMap[monthAbbr];
}

static std::string GetBuildTimestamp()
{
#ifdef __DATE__
    std::string date = __DATE__;
#else
    std::string date = "Jan 01 1970";
#endif

#ifdef __TIME__
    std::string time = __TIME__;
#else
    std::string time = "00:00:00";
#endif

    std::string month = getMonthNumber(date.substr(0, 3));
    std::string day = date.substr(4, 2);
    std::string year = date.substr(7, 4);

    if (day[0] == ' ')
        day[0] = '0';
    return year + "-" + month + "-" + day + "T" + time;
}

PyMethodDef* PyGetMillenniumModule();
extern "C" int Lua_OpenMillenniumLibrary(lua_State* L);
