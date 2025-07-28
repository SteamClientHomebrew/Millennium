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

#include "ffi.h"


/**
 * @brief Constructs a PythonGIL instance and initializes a new thread state.
 *
 * This constructor retrieves the main Python interpreter state and creates a new thread state for execution.
 */
PythonGIL::PythonGIL()
{
    m_mainInterpreter = PyInterpreterState_Main();
    m_interpreterThreadState = PyThreadState_New(m_mainInterpreter);
}

/**
 * @brief Acquires and locks the Python GIL for the current thread.
 *
 * This method ensures that the Python GIL is properly locked before executing Python code.
 */
const void PythonGIL::HoldAndLockGIL()
{
    PyEval_RestoreThread(m_interpreterThreadState);
    m_interpreterGIL = PyGILState_Ensure();
}

/**
 * @brief Acquires and locks the Python GIL for a specific thread.
 *
 * @param {PyThreadState*} threadState - The Python thread state to switch to.
 *
 * This function locks the GIL and swaps the thread state to the provided one, allowing safe execution
 * of Python code in a multi-threaded environment.
 */
const void PythonGIL::HoldAndLockGILOnThread(PyThreadState* threadState)
{
    PyEval_RestoreThread(m_interpreterThreadState);
    m_interpreterGIL = PyGILState_Ensure();
    PyThreadState_Swap(threadState);
}

/**
 * @brief Releases the Python GIL and deletes the current thread state.
 *
 * This destructor ensures that the thread state is properly cleared and deleted, preventing memory leaks.
 */
PythonGIL::~PythonGIL()
{
    PyThreadState_Clear(m_interpreterThreadState);
    PyThreadState_Swap(m_interpreterThreadState);

    PyGILState_Release(m_interpreterGIL);
    PyThreadState_DeleteCurrent();
}

/**
 * @brief Releases and unlocks the Python GIL.
 *
 * This function resets the shared instance of `PythonGIL`, effectively releasing the GIL and cleaning up resources.
 */
const void PythonGIL::ReleaseAndUnLockGIL()
{
    std::shared_ptr<PythonGIL> self = shared_from_this();
    self.reset();
}