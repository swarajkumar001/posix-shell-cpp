#pragma once
#include <string>

// Prints "username@system_name:dir> ". `dir` is already relative to the shell home ("~").
void displayPrompt(const std::string &dir);
