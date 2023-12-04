#include <metrics.hpp>
#include <Psapi.h>
#include <iostream>

ULONGLONG Metrics::SubtractTimes(const FILETIME one, const FILETIME two)
{
    LARGE_INTEGER a, b;
    a.LowPart = one.dwLowDateTime;
    a.HighPart = one.dwHighDateTime;

    b.LowPart = two.dwLowDateTime;
    b.HighPart = two.dwHighDateTime;

    return a.QuadPart - b.QuadPart;
}

double Metrics::getCpuUsage()
{
    auto cpuUsage = 0.0;

    FILETIME sysIdle, sysKernel, sysUser;

    if (GetSystemTimes(&sysIdle, &sysKernel, &sysUser) == 0) // GetSystemTimes func FAILED return value is zero;
        return 0;

    if (prevSysIdle.dwLowDateTime != 0 && prevSysIdle.dwHighDateTime != 0)
    {
        ULONGLONG sysIdleDiff, sysKernelDiff, sysUserDiff;
        sysIdleDiff = SubtractTimes(sysIdle, prevSysIdle);
        sysKernelDiff = SubtractTimes(sysKernel, prevSysKernel);
        sysUserDiff = SubtractTimes(sysUser, prevSysUser);

        ULONGLONG sysTotal = sysKernelDiff + sysUserDiff;
        ULONGLONG kernelTotal = sysKernelDiff - sysIdleDiff;

        if (sysTotal > 0)
            cpuUsage = (double)(((kernelTotal + sysUserDiff) * 100.0) / sysTotal);
    }

    prevSysIdle = sysIdle;
    prevSysKernel = sysKernel;
    prevSysUser = sysUser;

    return cpuUsage;
}

double Metrics::getMemoryUsage()
{
	HANDLE hProcess = GetCurrentProcess();
	if (hProcess == nullptr)
		return 0;

    PROCESS_MEMORY_COUNTERS_EX pmc;
	if (GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS) & pmc, sizeof(pmc)) == 0)
		return 0;

    return static_cast<double>((double)pmc.PrivateUsage) / (1024 * 1024);
}