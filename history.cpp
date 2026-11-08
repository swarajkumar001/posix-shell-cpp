#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>

const std::string historyFile = std::string(getenv("HOME")) + "/.my_shell_history";
std::vector<std::string> history;


// Load history from file up to last 20 commands..........
void loadHistory()
{
    history.clear();
    std::ifstream fin(historyFile);
    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty())
            history.push_back(line);
    }
    // Keep only last 20
    if (history.size() > 20)
        history.erase(history.begin(), history.end() - 20);
}

// saving history to the file.........
void saveHistory()
{
    std::ofstream fout(historyFile, std::ios::trunc);
    for (const auto &cmd : history)
        fout << cmd << std::endl;
}
// Add command to history (called after executing a command).........
void addToHistory(const std::string &cmd)
{
    if (cmd.empty())
        return;
    if (history.size() == 20)
        history.erase(history.begin());
    history.push_back(cmd);
    saveHistory();
}

// Print last 10 commands from history.........
void printHistory()
{
    int start = history.size() > 10 ? history.size() - 10 : 0;
    for (size_t i = start; i < history.size(); ++i)
        std::cout << (i + 1) << " " << history[i] << std::endl;
}
