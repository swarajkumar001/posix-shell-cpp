// search.cpp : recursive search for a file or folder under the current directory
#include "shell.h"

#include <cstring>
#include <dirent.h>
#include <iostream>
#include <sys/stat.h>

static bool searchDir(const std::string &dir, const std::string &target)
{
    DIR *dp = opendir(dir.c_str());
    if (!dp)
        return false;

    bool found = false;
    std::vector<std::string> subdirs;
    struct dirent *entry;
    while ((entry = readdir(dp)) != nullptr)
    {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (target == name)
        {
            found = true;
            break;
        }
        std::string path = dir + "/" + name;
        struct stat st;
        // lstat: don't follow symlinks, so symlink loops can't recurse forever
        if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            subdirs.push_back(path);
    }
    closedir(dp);

    if (found)
        return true;
    for (const auto &sub : subdirs)
        if (searchDir(sub, target))
            return true;
    return false;
}

void Shell::handleSearch(const std::vector<std::string> &args)
{
    if (args.size() != 2)
    {
        std::cerr << "Usage: search <file_or_folder_name>" << std::endl;
        return;
    }
    std::cout << (searchDir(".", args[1]) ? "True" : "False") << std::endl;
}
