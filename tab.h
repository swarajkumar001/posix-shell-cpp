#pragma once
#include <string>
#include <vector>

// Built-ins and executables in $PATH starting with prefix (sorted, unique).
std::vector<std::string> autocompleteCommands(const std::string &prefix);

// Files/directories in the current directory starting with prefix (sorted).
std::vector<std::string> autocompleteFiles(const std::string &prefix);

// Longest common prefix of all matches.
std::string longestCommonPrefix(const std::vector<std::string> &matches);
