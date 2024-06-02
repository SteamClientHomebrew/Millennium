#include "ffi.hpp"

PythonGIL::PythonGIL()
{
    m_mainInterpreter = PyInterpreterState_Main();
    m_interpreterThreadState = PyThreadState_New(m_mainInterpreter);
}

const void PythonGIL::HoldAndLockGIL()
{
    PyEval_RestoreThread(m_interpreterThreadState);
    m_interpreterGIL = PyGILState_Ensure();
}

const void PythonGIL::HoldAndLockGILOnThread(PyThreadState* threadState)
{
    PyEval_RestoreThread(m_interpreterThreadState);
    m_interpreterGIL = PyGILState_Ensure();
    PyThreadState_Swap(threadState);
}

PythonGIL::~PythonGIL()
{
    PyThreadState_Clear(m_interpreterThreadState);
    PyThreadState_Swap(m_interpreterThreadState);

    PyGILState_Release(m_interpreterGIL);
    PyThreadState_DeleteCurrent();
}

const void PythonGIL::ReleaseAndUnLockGIL()
{
    std::shared_ptr<PythonGIL> self = shared_from_this();
    self.reset();
}