// system_command.cpp : execution of external programs (vim, gcc, ./a.out, ...)
#include "shell.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>

// Called in a forked child: replaces it with the requested program.
void Shell::execExternal(const std::vector<std::string> &argv)
{
    std::vector<std::string> expanded;
    expanded.reserve(argv.size());
    for (const auto &a : argv)
        expanded.push_back(expandHome(a));

    std::vector<char *> cargv;
    for (auto &a : expanded)
        cargv.push_back(const_cast<char *>(a.c_str()));
    cargv.push_back(nullptr);

    execvp(cargv[0], cargv.data());

    if (errno == ENOENT)
        std::cerr << argv[0] << ": command not found" << std::endl;
    else
        std::cerr << argv[0] << ": " << strerror(errno) << std::endl;
    _exit(127);
}
