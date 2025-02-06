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
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

class StartupParameters 
{
public:

    StartupParameters() 
    {
        this->ParseCommandLine();
    }

    bool HasArgument(const std::string& targetArgument) const 
    {
        for (const std::string& argument : m_argumentList) 
        {
            if (argument == targetArgument) 
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> GetArgumentList()
    {
        return this->m_argumentList;
    }

private:

    void ParseCommandLine() 
    {
#ifdef _WIN32
        int argc = 0;
        wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argvW) return;

        for (int i = 0; i < argc; ++i) 
        {
            int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
            if (len <= 0) return;

            std::vector<char> buffer(len);
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, buffer.data(), len, NULL, NULL);

            m_argumentList.emplace_back(buffer.data());
        }
        LocalFree(argvW);
#endif
    }

    std::vector<std::string> m_argumentList;
};