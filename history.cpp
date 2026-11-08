// history.cpp : persistent command history (stored in ~/.my_shell_history)
#include "shell.h"

#include <fstream>
#include <iostream>

static const size_t HISTORY_MAX = 20;  // commands remembered
static const size_t HISTORY_SHOW = 10; // shown by plain "history"

void Shell::loadHistory()
{
    history.clear();
    std::ifstream in(history_file);
    std::string line;
    while (std::getline(in, line))
        if (!line.empty())
            history.push_back(line);
    if (history.size() > HISTORY_MAX)
        history.erase(history.begin(), history.end() - HISTORY_MAX);
}

void Shell::saveHistory()
{
    std::ofstream out(history_file, std::ios::trunc);
    if (!out)
        return;
    for (const auto &cmd : history)
        out << cmd << '\n';
}

// Stores a command (consecutive duplicates are skipped), dropping the oldest
// one once 20 are stored, and saves to disk so history survives restarts.
void Shell::addToHistory(const std::string &cmd)
{
    size_t b = cmd.find_first_not_of(" \t");
    if (b == std::string::npos)
        return;
    size_t e = cmd.find_last_not_of(" \t");
    std::string clean = cmd.substr(b, e - b + 1);

    if (!history.empty() && history.back() == clean)
        return;
    history.push_back(clean);
    if (history.size() > HISTORY_MAX)
        history.erase(history.begin());
    saveHistory();
}

// history [n] : prints the latest n commands (default 10).
void Shell::handleHistory(const std::vector<std::string> &args)
{
    if (args.size() > 2)
    {
        std::cerr << "history: too many arguments" << std::endl;
        return;
    }

    size_t n = HISTORY_SHOW;
    if (args.size() == 2)
    {
        try
        {
            size_t pos = 0;
            long v = std::stol(args[1], &pos);
            if (pos != args[1].size() || v < 0)
                throw 0;
            n = static_cast<size_t>(v);
        }
        catch (...)
        {
            std::cerr << "history: " << args[1] << ": numeric argument required" << std::endl;
            return;
        }
    }

    if (n > history.size())
        n = history.size();
    for (size_t i = history.size() - n; i < history.size(); ++i)
        std::cout << history[i] << '\n';
    std::cout.flush();
}
