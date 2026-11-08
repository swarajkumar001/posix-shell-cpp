#include "shell.h"   // Include the shell header for Shell class definition.........
#include <iostream>  // For std::cout, std::cerr
#include <unistd.h>  // For chdir, getcwd


// Change directory command implementation.........
void Shell::handleCD(const std::vector<std::string> &args)
{
    std::string target;
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
    {
        perror("getcwd");
        return;
    }
    std::string current_dir = cwd;

    if (args.size() == 1)
    {
        target = getenv("HOME") ? getenv("HOME") : "/";
    }
    else
    {
        target = args[1];
        if (target == "~")
        {
            target = getenv("HOME") ? getenv("HOME") : "/";
        }
        else if (target == "-")
        {
            if (prev_dir.empty())
            {
                std::cerr << "cd: OLDPWD not set" << std::endl;
                return;
            }
            target = prev_dir;
            std::cout << target << std::endl;
        }
    }

    // Change directory.......

    if (chdir(target.c_str()) != 0)
    {
        perror("cd");
    }
    else
    {
        prev_dir = current_dir;
    }
}
