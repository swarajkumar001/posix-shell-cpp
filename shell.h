#ifndef SHELL_H
#define SHELL_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

class Shell
{
private:
    bool running;
    std::vector<std::string> history;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> env_vars;
    std::vector<std::pair<pid_t, std::string>> background_jobs;
    std::string custom_prompt;
    std::string prev_dir;

public:
    Shell();
    int run();

    void executeCommand(const std::string &input);

    // Built-ins
    void handleCD(const std::vector<std::string> &args);
    void handlePWD();
    void handleEcho(const std::vector<std::string> &args);
    void handleHistory();
    void handleClear();
    void handleExit();
    void handlePinfo(const std::vector<std::string> &args);
    void handleLS(const std::vector<std::string> &args);

    void handleSearch(const std::vector<std::string> &args);

    void executeWithRedirection(const std::vector<std::string> &args, bool background);

    int setupRedirection(const std::vector<std::string> &args, int &saved_stdin, int &saved_stdout);
    void restoreRedirection(int saved_stdin, int saved_stdout);

    // Helpers
    std::vector<std::string> tokenize(const std::string &input);
    void executePipeline(const std::string &input);
    void setForegroundPGID(pid_t pgid);
    void addToHistory(const std::string &command);
    void cleanup();

    // External/system
    void executeExternal(const std::vector<std::string> &args, bool background = false);
    void setPrompt(const std::string &prompt);

    // (optional) parsing helpers
    bool hasPipes(const std::string &input);
    bool hasRedirection(const std::string &input);
};

#endif // SHELL_H
