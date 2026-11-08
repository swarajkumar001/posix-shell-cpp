// tab.cpp : TAB autocompletion for commands and files in the current directory
#include "tab.h"
#include "shell.h"

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <set>
#include <sys/stat.h>

static void addDirMatches(const std::string &dir, const std::string &prefix, std::set<std::string> &out)
{
    DIR *dp = opendir(dir.c_str());
    if (!dp)
        return;
    struct dirent *entry;
    while ((entry = readdir(dp)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name.compare(0, prefix.size(), prefix) == 0 && name != "." && name != "..")
            out.insert(name);
    }
    closedir(dp);
}

std::vector<std::string> autocompleteCommands(const std::string &prefix)
{
    std::set<std::string> matches;
    for (const auto &cmd : BUILTINS)
        if (cmd.compare(0, prefix.size(), prefix) == 0)
            matches.insert(cmd);

    const char *path_env = getenv("PATH");
    if (path_env)
    {
        std::string path(path_env);
        size_t start = 0;
        while (true)
        {
            size_t end = path.find(':', start);
            addDirMatches(path.substr(start, end - start), prefix, matches);
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    return std::vector<std::string>(matches.begin(), matches.end());
}

std::vector<std::string> autocompleteFiles(const std::string &prefix)
{
    std::set<std::string> found;
    addDirMatches(".", prefix, found);

    std::vector<std::string> matches;
    for (const auto &name : found)
        if (name[0] != '.' || (!prefix.empty() && prefix[0] == '.')) // hide dotfiles unless asked
            matches.push_back(name);
    return matches;
}

std::string longestCommonPrefix(const std::vector<std::string> &matches)
{
    if (matches.empty())
        return "";
    std::string prefix = matches[0];
    for (size_t i = 1; i < matches.size() && !prefix.empty(); ++i)
    {
        size_t j = 0;
        while (j < prefix.size() && j < matches[i].size() && prefix[j] == matches[i][j])
            ++j;
        prefix.resize(j);
    }
    return prefix;
}

// Completes the word under the cursor:
//   one match       -> completes it (adds "/" for directories, " " otherwise)
//   several matches -> fills the common prefix, or lists them all if there is nothing to fill
void Shell::handleTab(std::string &buf)
{
    size_t word_start = buf.find_last_of(" \t|;<>");
    word_start = (word_start == std::string::npos) ? 0 : word_start + 1;
    std::string word = buf.substr(word_start);

    // Command position: start of line, or right after | or ;
    bool is_command = true;
    if (word_start > 0)
    {
        size_t prev = buf.find_last_not_of(" \t", word_start - 1);
        is_command = (prev == std::string::npos || buf[prev] == '|' || buf[prev] == ';');
    }

    if (is_command && word.empty())
    {
        std::cout << '\a' << std::flush;
        return;
    }

    std::vector<std::string> matches = is_command ? autocompleteCommands(word) : autocompleteFiles(word);
    if (matches.empty())
    {
        std::cout << '\a' << std::flush;
        return;
    }

    if (matches.size() == 1)
    {
        std::string completion = matches[0].substr(word.size());
        struct stat st;
        bool is_dir = !is_command && stat(matches[0].c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        completion += is_dir ? "/" : " ";
        buf += completion;
        std::cout << completion << std::flush;
        return;
    }

    std::string common = longestCommonPrefix(matches);
    if (common.size() > word.size())
    {
        std::string extra = common.substr(word.size());
        buf += extra;
        std::cout << extra << std::flush;
        return;
    }

    std::cout << '\n';
    for (size_t i = 0; i < matches.size(); ++i)
        std::cout << matches[i] << (i + 1 < matches.size() ? "  " : "\n");
    redrawLine(buf);
}
