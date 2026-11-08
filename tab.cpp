#include "tab.h"
#include <dirent.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <set>

// List of built-in commands (add more as needed)
static const std::vector<std::string> builtins = {
    "cd", "ls", "echo", "pinfo", "history", "fg", "bg", "exit"};

std::vector<std::string> autocompleteCommands(const std::string &prefix)
{
    std::set<std::string> matches;
    // Built-in commands
    for (const auto &cmd : builtins)
    {
        if (cmd.find(prefix) == 0)
            matches.insert(cmd);
    }
    // Search $PATH for executables
    const char *path_env = getenv("PATH");
    if (path_env)
    {
        std::string path(path_env);
        size_t start = 0, end = 0;
        while ((end = path.find(':', start)) != std::string::npos)
        {
            std::string dir = path.substr(start, end - start);
            DIR *dp = opendir(dir.c_str());
            if (dp)
            {
                struct dirent *entry;
                while ((entry = readdir(dp)))
                {
                    std::string name(entry->d_name);
                    if (name.find(prefix) == 0)
                        matches.insert(name);
                }
                closedir(dp);
            }
            start = end + 1;
        }
        // Last PATH segment
        std::string dir = path.substr(start);
        DIR *dp = opendir(dir.c_str());
        if (dp)
        {
            struct dirent *entry;
            while ((entry = readdir(dp)))
            {
                std::string name(entry->d_name);
                if (name.find(prefix) == 0)
                    matches.insert(name);
            }
            closedir(dp);
        }
    }
    return std::vector<std::string>(matches.begin(), matches.end());
}

std::vector<std::string> autocompleteFiles(const std::string &prefix)
{
    std::vector<std::string> matches;
    DIR *dp = opendir(".");
    if (dp)
    {
        struct dirent *entry;
        while ((entry = readdir(dp)))
        {
            std::string name(entry->d_name);
            if (name.find(prefix) == 0)
                matches.push_back(name);
        }
        closedir(dp);
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::string longestCommonPrefix(const std::vector<std::string> &matches)
{
    if (matches.empty())
        return "";
    std::string prefix = matches[0];
    for (size_t i = 1; i < matches.size(); ++i)
    {
        size_t j = 0;
        while (j < prefix.size() && j < matches[i].size() && prefix[j] == matches[i][j])
            ++j;
        prefix = prefix.substr(0, j);
        if (prefix.empty())
            break;
    }
    return prefix;
}
