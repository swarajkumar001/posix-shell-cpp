// shell.cpp
#include "display.h"
#include "shell.h"
#include "bg_fg.h"
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>
#include <termios.h>
// System/STD includes
#include <iostream>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <unistd.h>
#include <iostream>

using std::cerr;

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;
#include "display.h"


//Shell class implementation.........
Shell::Shell() : running(true) {}

int Shell::run()
{
    // Setup signal handlers for Ctrl-C and Ctrl-Z
    setup_signal_handlers();
    struct sigaction sa_int{};
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_tstp{};
    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, nullptr);

    // Raw mode setup
    struct termios orig_termios, raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    std::string input;
    size_t history_index = history.size();
    std::string current_buffer;

    while (running)
    {
        ::displayPrompt();
        input.clear();
        current_buffer.clear();
        history_index = history.size();
        char c;
        size_t max_buffer_len = 0;
        while (true)
        {
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n <= 0)
                break;
            if (c == '\n')
            {
                std::cout << std::endl;
                input = current_buffer;
                break;
            }
            else if (c == 4) // Ctrl-D
            {
                std::cout << "Logout" << std::endl;
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                return 0;
            }
            else if (c == 127) // Backspace
            {
                if (!current_buffer.empty())
                {
                    current_buffer.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            }
            else if (c == '\x1b') // Escape sequence
            {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1)
                {
                    if (seq[0] == '[')
                    {
                        if (seq[1] == 'A') // UP
                        {
                            if (history.size() > 0 && history_index > 0)
                            {
                                history_index--;
                                // Clear current line (use max_buffer_len)
                                std::cout << "\r";
                                ::displayPrompt();
                                size_t clear_len = std::max(current_buffer.size(), max_buffer_len);
                                for (size_t i = 0; i < clear_len; ++i)
                                    std::cout << ' ';
                                std::cout << "\r"; //carriage return.....
                                ::displayPrompt();
                                current_buffer = history[history_index];
                                std::cout << current_buffer << std::flush;
                                max_buffer_len = std::max(max_buffer_len, current_buffer.size());
                            }
                        }
                        else if (seq[1] == 'B') // DOWN
                        {
                            if (history_index < history.size() - 1)
                            {
                                history_index++;
                                std::cout << "\r";
                                ::displayPrompt();
                                size_t clear_len = std::max(current_buffer.size(), max_buffer_len);
                                for (size_t i = 0; i < clear_len; ++i)
                                    std::cout << ' ';
                                std::cout << "\r";
                                ::displayPrompt();
                                current_buffer = history[history_index];
                                std::cout << current_buffer << std::flush;
                                max_buffer_len = std::max(max_buffer_len, current_buffer.size());
                            }
                            else if (history_index == history.size() - 1)
                            {
                                history_index++;
                                std::cout << "\r";
                                ::displayPrompt();
                                size_t clear_len = std::max(current_buffer.size(), max_buffer_len);
                                for (size_t i = 0; i < clear_len; ++i)
                                    std::cout << ' ';
                                std::cout << "\r";
                                ::displayPrompt();
                                current_buffer.clear();
                                std::cout << std::flush;
                            }
                        }
                    }
                }
            }
            else
            {
                current_buffer += c;
                std::cout << c << std::flush;
                max_buffer_len = std::max(max_buffer_len, current_buffer.size());
            }
        }
        if (input.empty())
            continue;
            //add to history..
        addToHistory(input);
        // support for ; separated commands
        size_t start = 0;
        while (true)
        {
            size_t pos = input.find(';', start);
            string part = (pos == string::npos) ? input.substr(start)
                                                : input.substr(start, pos - start);
            if (!part.empty())
            {
                executeCommand(part);
            }
            if (pos == string::npos)
                break;
            start = pos + 1;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    cleanup();
    return 0;
}

void Shell::executeCommand(const string &input)
{
    vector<string> args = tokenize(input);
    if (args.empty())
        return;

    // background if last token is &
    bool background = false;
    if (!args.empty() && args.back() == "&")
    {
        background = true;
        args.pop_back();
        if (args.empty())
            return;
    }

    const string &command = args[0];

    bool has_redir = false;
    std::vector<std::string> cmd_args;
    std::string input_file, output_file;
    bool append = false;

    // Parse redirection and build cmd_args
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "<" && i + 1 < args.size())
        {
            has_redir = true;
            input_file = args[i + 1];
            ++i;
        }
        else if (args[i] == ">" && i + 1 < args.size())
        {
            has_redir = true;
            output_file = args[i + 1];
            ++i;
        }
        else if (args[i] == ">>" && i + 1 < args.size())
        {
            has_redir = true;
            output_file = args[i + 1];
            append = true;
            ++i;
        }
        else
        {
            cmd_args.push_back(args[i]);
        }
    }

    int saved_stdin = -1, saved_stdout = -1;
    // If the command contains a pipeline, do NOT apply top-level redirection in the
    // parent shell process. Per-segment redirections (like "cmd < in | cmd > out") are
    // handled inside executePipeline for each child. Apply redirection here only when
    // there are no pipes.
    if (has_redir && !hasPipes(input))
    {
        // Setup redirection
        saved_stdin = dup(STDIN_FILENO);
        saved_stdout = dup(STDOUT_FILENO);

        if (!input_file.empty())
        {
            int fd = open(input_file.c_str(), O_RDONLY);
            if (fd < 0)
            {
                std::cerr << "Input file not found: " << input_file << std::endl;
                // Restore and return
                if (saved_stdin != -1)
                {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                if (saved_stdout != -1)
                {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (!output_file.empty())
        {
            int fd;
            if (append)
            {
                fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            }
            else
            {
                fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (fd < 0)
            {
                std::cerr << "Output file error: " << output_file << std::endl;
                // Restore and return
                if (saved_stdin != -1)
                {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                if (saved_stdout != -1)
                {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
    }

    // Only pass cmd_args to built-ins and external commands
    if (command == "cd")
    {
        handleCD(cmd_args);
    }
    else if (command == "pwd")
    {
        handlePWD();
    }
    else if (command == "echo")
    {
        handleEcho(cmd_args);
    }
    else if (command == "history")
    {
        handleHistory();
    }
    else if (command == "clear")
    {
        handleClear();
    }
    else if (command == "exit")
    {
        handleExit();
    }
    else if (command == "ls")
    {
        handleLS(cmd_args);
    }
    else if (command == "pinfo")
    {
        handlePinfo(cmd_args);
    }
    else if (command == "search")
    {
        handleSearch(cmd_args);
    }
    else if (hasPipes(input))
    {
        executePipeline(input);
    }
    else
    {
        // executeExternal should receive the cleaned arguments (cmd_args)
        // cmd_args contains the original args with redirection tokens removed
        if (!cmd_args.empty())
            executeExternal(cmd_args, background);
        else
            executeExternal(args, background);
    }

    // Restore redirection after command execution
    if (has_redir)
    {
        if (saved_stdin != -1)
        {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1)
        {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
    }
}

//external command for background............

void Shell::executeExternal(const vector<string> &args, bool background)
{
    if (args.empty())
        return;

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return;
    }

    if (pid == 0)
    {
        // Child: build argv and exec
        // Put child into its own process group so signals can be sent to the job
        setpgid(0, 0);
        std::vector<std::string> expanded_args;
        const char *home = getenv("HOME");
        for (const auto &a : args)
        {
            if (!a.empty() && a[0] == '~' && home)
            {
                if (a.size() == 1)
                    expanded_args.push_back(home);
                else if (a[1] == '/')
                    expanded_args.push_back(std::string(home) + a.substr(1));
                else
                    expanded_args.push_back(a); // e.g. ~user not supported
            }
            else
            {
                expanded_args.push_back(a);
            }
        }
        std::vector<char *> exec_args;
        exec_args.reserve(expanded_args.size() + 1);
        for (const auto &a : expanded_args)
            exec_args.push_back(const_cast<char *>(a.c_str()));
        exec_args.push_back(nullptr);

        if (execvp(exec_args[0], exec_args.data()) == -1)
        {
            perror("execvp failed");
            _exit(127);
        }
    }
    else
    {
        // Parent
        // Set child's pgid in parent
        setpgid(pid, pid);
        if (background)
        {
            std::cout << "Process started in background with PID: " << pid << std::endl;
            // Optional: record in background_jobs
        }
        else
        {
            // set global foreground pgid so handlers can signal the job
            fg_pgid = pid;
            int status = 0;
            waitpid(pid, &status, 0);
            fg_pgid = 0;
        }
    }
}

// --------- helpers ---------

// whitespace tokenizer; no quotes/escapes per assignment
std::vector<std::string> Shell::tokenize(const std::string &input)
{
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;
    char quote_char = '\0';
    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (!in_quotes && (c == '"' || c == '\''))
        {
            in_quotes = true;
            quote_char = c;
            continue;
        }
        if (in_quotes && c == quote_char)
        {
            in_quotes = false;
            continue;
        }
        if (!in_quotes && (c == ' ' || c == '\t' || c == '\n'))
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

void Shell::addToHistory(const std::string &command)
{
    history.push_back(command);
    if (history.size() > 20)
        history.erase(history.begin());
}

void Shell::cleanup()
{
    // No-op for now; add SIGCHLD reaping if tracking background_jobs
}

//
// Parsing helpers
bool Shell::hasPipes(const std::string &s)
{
    return s.find('|') != std::string::npos;
}


// Redirection helpers
bool Shell::hasRedirection(const std::string &s)
{
    return s.find('<') != std::string::npos || s.find('>') != std::string::npos;
}


// Set custom prompt...
void Shell::setPrompt(const std::string &p)
{
    custom_prompt = p;
}


// Set the global foreground pgid used by signal handlers.......
void Shell::setForegroundPGID(pid_t pgid)
{
    // set the global foreground pgid used by signal handlers
    fg_pgid = pgid;
}


// Built-in command implementations.........
void Shell::handlePWD()
{
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)))
    {
        std::cout << cwd << std::endl;
    }
    else
    {
        perror("getcwd");
    }
}

// Echo command implementation.........
void Shell::handleEcho(const std::vector<std::string> &args)
{
    for (size_t i = 1; i < args.size(); ++i)
    {
        std::cout << args[i];
        if (i + 1 < args.size())
            std::cout << " ";
    }
    std::cout << std::endl;
}


// History command implementation.........
void Shell::handleHistory()
{
    // Check if user requested 'history N'
    std::string last_input;
    if (!history.empty())
        last_input = history.back();
    size_t n = 10;
    if (last_input.find("history ") == 0)
    {
        std::string num = last_input.substr(8);
        try
        {
            n = std::stoul(num);
        }
        catch (...)
        {
            n = 10;
        }
    }
    size_t start = history.size() > n ? history.size() - n : 0;
    for (size_t i = start; i < history.size(); ++i)
        std::cout << history[i] << std::endl;
}

// Clear command implementation.........
void Shell::handleClear()
{
    std::cout << "\033[2J\033[H";
}


// Exit command implementation.........
void Shell::handleExit()
{
    running = false;
}


// LS command implementation.........
void Shell::handleLS(const std::vector<std::string> &args)
{
    std::vector<char *> exec_args;
    exec_args.push_back(const_cast<char *>("ls"));
    for (size_t i = 1; i < args.size(); ++i)
    {
        exec_args.push_back(const_cast<char *>(args[i].c_str()));
    }
    exec_args.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp("ls", exec_args.data());
        perror("execvp");
        _exit(127);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
    }
    else
    {
        perror("fork");
    }
}

int Shell::setupRedirection(const std::vector<std::string> &args, int &saved_stdin, int &saved_stdout)
{
    saved_stdin = dup(STDIN_FILENO);
    saved_stdout = dup(STDOUT_FILENO);

    int in_fd = -1, out_fd = -1;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "<" && i + 1 < args.size())
        {
            in_fd = open(args[i + 1].c_str(), O_RDONLY);
            if (in_fd < 0)
            {
                std::cerr << "Input file not found: " << args[i + 1] << std::endl;
                return -1;
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        else if (args[i] == ">" && i + 1 < args.size())
        {
            out_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0)
            {
                std::cerr << "Output file error: " << args[i + 1] << std::endl;
                return -1;
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        else if (args[i] == ">>" && i + 1 < args.size())
        {
            out_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (out_fd < 0)
            {
                std::cerr << "Output file error: " << args[i + 1] << std::endl;
                return -1;
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
    }
    return 0;
}


// Restore redirection....
void Shell::restoreRedirection(int saved_stdin, int saved_stdout)
{
    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin);
    close(saved_stdout);
}
