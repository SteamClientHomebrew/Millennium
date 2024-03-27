#include <iostream>
#include <dlfcn.h>

class SteamLaunchNotifier {
public:
    SteamLaunchNotifier() {
        std::cout << "[bootstrapper] warming wrapper core..." << std::endl;

        dlopen("/lib/x86_64-linux-gnu/libxcb.so.1", RTLD_NOW);

        if (!(void*)dlopen("libmillennium.so", RTLD_LAZY))
        {
            std::cerr << "[bootstrapper] error loading millennium -> " << dlerror() << std::endl;
        }
    }
    ~SteamLaunchNotifier() {
        std::cout << "[bootstrapper] unloading..." << std::endl;
    }
};

SteamLaunchNotifier notifier;

// [[maybe_unused]] static void __attribute__((constructor)) initialize_millennium() {
//    std::cout << "[bootloader] module loaded into memory..." << std::endl;
// }