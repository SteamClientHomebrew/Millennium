#include <Windows.h>

class Metrics {
public:
    Metrics() {}
    ~Metrics() {}

    double getCpuUsage();

    double getMemoryUsage();
private:
    FILETIME prevSysIdle, prevSysKernel, prevSysUser;

    ULONGLONG SubtractTimes(const FILETIME one, const FILETIME two);
};