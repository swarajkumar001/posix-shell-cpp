#pragma once
#include <string>
#include <vector>

// Returns a list of matching commands for prefix
std::vector<std::string> autocompleteCommands(const std::string &prefix);

// Returns a list of matching files/directories for prefix in current directory
std::vector<std::string> autocompleteFiles(const std::string &prefix);

// Returns the longest common prefix among matches
std::string longestCommonPrefix(const std::vector<std::string> &matches);
