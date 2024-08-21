#include <fstream>
#ifdef __linux__
#include <sys/utsname.h>
#include <sys/auxv.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <iostream>
#include <unistd.h>


std::string GetLinuxDistro() 
{
    #ifdef __linux__
    std::string line, distro;
    std::ifstream file("/etc/os-release");

    while (std::getline(file, line)) 
    {
        if (line.find("ID=") == 0) 
        {
            distro = line.substr(3);
            break;
        }
    }

    return distro;
    #elif __APPLE__
    return "osx";
    #endif
}

std::string GetSystemArchitecture()
{
    #ifdef __linux__
    struct utsname info;
    uname(&info);

    return info.machine;

    #elif __APPLE__

    char arch[256];
    size_t size = sizeof(arch);

    if (sysctlbyname("machdep.cpu.brand_string", arch, &size, NULL, 0) == 0) {
        return arch;
    } else {
        return "unknown arch!";
    }
    #endif
}