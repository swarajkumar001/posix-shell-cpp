#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <termios.h>

// Names of every command implemented inside the shell (shell.cpp).
extern const std::vector<std::string> BUILTINS;

// One stage of a pipeline: the argument vector plus its < > >> redirections.
struct Command
{
    std::vector<std::string> argv;
    std::string in_file;
    std::string out_file;
    bool append = false;
};

// A background or stopped job, identified by its process group id.
struct Job
{
    pid_t pgid;
    std::string name;
    bool stopped;
};

class Shell
{
public:
    Shell();
    int run();

private:
    bool running;
    bool interactive;            // stdin is a terminal
    pid_t shell_pgid;
    pid_t orig_fg_pgid;          // terminal owner before the shell started
    struct termios orig_termios; // cooked terminal settings

    std::string home_dir;     // directory the shell was started from, shown as "~"
    std::string prev_dir;     // for "cd -"
    std::string history_file; // persistent history across sessions
    std::vector<std::string> history;
    std::vector<Job> jobs;

    // shell.cpp : prompt, line editing and dispatch
    void printPrompt();
    void enableRawMode();
    void disableRawMode();
    bool readLine(std::string &line);
    void redrawLine(const std::string &buf);
    void executeLine(const std::string &line);
    void executeCommand(const std::string &cmdline);
    std::vector<std::string> tokenize(const std::string &input);
    bool isBuiltin(const std::string &name) const;
    void runBuiltin(const std::vector<std::string> &args);
    std::string expandHome(const std::string &path) const;
    std::string displayPath(const std::string &path) const;
    void cleanup();

    // tab.cpp : TAB autocompletion
    void handleTab(std::string &buf);

    // io_pipe.cpp : redirection and pipelines
    bool parseCommand(const std::vector<std::string> &tokens, Command &cmd);
    bool applyRedirection(const Command &cmd);
    void executePipeline(const std::vector<Command> &cmds, bool background, const std::string &name);

    // system_command.cpp : exec of external programs
    [[noreturn]] void execExternal(const std::vector<std::string> &argv);

    // bg_fg.cpp : job control
    void waitForeground(pid_t pgid, const std::string &name, bool resume = false);
    void reapBackgroundJobs();
    int findJob(const std::vector<std::string> &args, const char *cmd);
    void handleJobs();
    void handleFg(const std::vector<std::string> &args);
    void handleBg(const std::vector<std::string> &args);

    // built-in commands
    void handleCD(const std::vector<std::string> &args);     // cd.cpp
    void handlePWD();                                        // cd.cpp
    void handleEcho(const std::vector<std::string> &args);   // echo.cpp
    void handleLS(const std::vector<std::string> &args);     // ls.cpp
    void handlePinfo(const std::vector<std::string> &args);  // pinfo.cpp
    void handleSearch(const std::vector<std::string> &args); // search.cpp

    // history.cpp
    void loadHistory();
    void saveHistory();
    void addToHistory(const std::string &cmd);
    void handleHistory(const std::vector<std::string> &args);
};

#endif // SHELL_H
