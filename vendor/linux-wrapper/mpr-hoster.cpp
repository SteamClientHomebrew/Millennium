#include <iostream>
#include <dlfcn.h>

class SteamLaunchNotifier {
public:
    SteamLaunchNotifier() {
        std::cout << "[MILLENNIUM-BOOTSTRAPPER] warming wrapper core..." << std::endl;

        if (!(void*)dlopen("libmillennium.so", RTLD_LAZY))
        {
            std::cerr << "[MILLENNIUM-BOOTSTRAPPER] error loading millennium -> " << dlerror() << std::endl;
        }
    }
    ~SteamLaunchNotifier() {
        std::cout << "exiting millennium" << std::endl;
    }
};

SteamLaunchNotifier notifier;

//[[maybe_unused]] static void __attribute__((constructor)) initialize_millennium() {
//    std::cout << "awdawdawd" << std::endl;
//}