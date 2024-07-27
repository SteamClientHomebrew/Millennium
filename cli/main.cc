#include <iostream>
#include <algorithm> 
#include "params.h"

std::function<void()> FindFunction(const std::string& arg, const std::vector<ParameterProps>& argMap) {
    auto it = std::find_if(flagArguments.begin(), flagArguments.end(), [&arg](const ParameterProps& p) {
        return std::find(p.ids.begin(), p.ids.end(), arg) != p.ids.end();
    });

    if (it != flagArguments.end()) {
        return it->callback;
    }

    return nullptr;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        PrintMillennium();
        return 0;
    }

    // get the first argument
    auto func = FindFunction(argv[1], flagArguments);

    if (func) func();
    else {
        std::cout << "Argument not found: " << argv[1] << std::endl;
    }

    return 0;
}