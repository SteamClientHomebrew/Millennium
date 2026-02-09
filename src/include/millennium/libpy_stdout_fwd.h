/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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
 * bind_stdout.h
 * @brief This file is responsible for redirecting the Python stdout and stderr to the logger.
 * @note You shouldn't rely print() in python, use "PluginUtils" utils module, this is just a catch-all for any print() calls that may be made.
 */

#pragma once

#include <Python.h>

/**
 * @brief Write a message buffer to the logger. The buffer is un-flushed.
 *
 * @param pname The name of the plugin.
 * @param message The message to be printed.
 */
extern "C" void PrintPythonMessage(std::string pname, const char* message);

/**
 * @brief Write an error message buffer to the logger.
 * We dont use the plugin here as most messages coming from stderr are flushed every character.
 *
 * ex. 123 would print plugin name 1\n plugin name 2\n plugin name 3\n
 */
extern "C" void PrintPythonError(std::string pname, const char* message);

/** Forward messages to respective logger type. */
extern "C" PyObject* CustomStdoutWrite(PyObject* self, PyObject* args);
extern "C" PyObject* CustomStderrWrite(PyObject* self, PyObject* args);

extern "C" PyObject* PyInit_CustomStderr(void);
extern "C" PyObject* PyInit_CustomStdout(void);

/** @brief Redirects the Python stdout and stderr to the logger. */
extern "C" void RedirectOutput();
