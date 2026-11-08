#include "shell.h"
#include <iostream>
#include <vector>

void handleEcho(const std::vector<std::string>& args) {
    // Start from index 1 to skip command name "echo"
    for (size_t i = 1; i < args.size(); ++i) {
        std::cout << args[i];
        if (i < args.size() - 1) {
            std::cout << " ";  // print space between arguments
        }
    }
    std::cout << std::endl;
}
