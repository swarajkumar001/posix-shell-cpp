// cd.cpp : cd and pwd built-ins
#include "shell.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <iostream>
#include <unistd.h>

// cd [dir] : supports ".", "..", "-", "~" and "~/path". No argument = shell home.
void Shell::handleCD(const std::vector<std::string> &args)
{
    if (args.size() > 2)
    {
        std::cerr << "Invalid arguments" << std::endl;
        return;
    }

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("cd: getcwd");
        return;
    }

    std::string target;
    bool print_dir = false;
    if (args.size() == 1)
    {
        target = home_dir;
    }
    else if (args[1] == "-")
    {
        if (prev_dir.empty())
        {
            std::cerr << "cd: no previous directory" << std::endl;
            return;
        }
        target = prev_dir;
        print_dir = true;
    }
    else
    {
        target = expandHome(args[1]);
    }

    if (chdir(target.c_str()) != 0)
    {
        std::cerr << "cd: " << (args.size() > 1 ? args[1] : target) << ": " << strerror(errno) << std::endl;
        return;
    }
    prev_dir = cwd;

    if (print_dir)
        handlePWD();
}

void Shell::handlePWD()
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
        std::cout << cwd << std::endl;
    else
        perror("pwd");
}
