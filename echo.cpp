// echo.cpp : echo built-in
#include "shell.h"

#include <iostream>

// Prints the arguments separated by single spaces; extra spaces/tabs between
// words are already collapsed by the tokenizer.
void Shell::handleEcho(const std::vector<std::string> &args)
{
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (i > 1)
            std::cout << ' ';
        std::cout << args[i];
    }
    std::cout << std::endl;
}
