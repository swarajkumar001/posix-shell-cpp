#include "shell.h"
#include <iostream>
#include <filesystem>

//implementing the search to check if file exist or not in current directory...
void Shell::handleSearch(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: search <filename_or_folder>" << std::endl;
        return;
    }
    // Get the target filename or folder name..........
    std::string target = args[1];
    bool found = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())) {
        if (entry.path().filename() == target) {
            found = true; // File or folder found.........
            break;
        }
    }
    std::cout << (found ? "True" : "False") << std::endl;
}