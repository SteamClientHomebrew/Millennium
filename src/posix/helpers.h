#include <fstream>
#include <sys/utsname.h>
#include <sys/auxv.h>
#include <iostream>
#include <unistd.h>

std::string GetLinuxDistro() 
{
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
}

std::string GetSystemArchitecture()
{
    struct utsname info;
    uname(&info);

    return info.machine;
}