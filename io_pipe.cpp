#include "shell.h"
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cerrno>

// Execute a command with possible input/output redirection

void Shell::executeWithRedirection(const std::vector<std::string> &args, bool background)
{
    std::vector<std::string> cmd_args;
    std::string input_file, output_file;
    bool append = false;

    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "<" && i + 1 < args.size())
        {
            input_file = args[i + 1]; // Input redirection
            ++i;
        }
        else if (args[i] == ">" && i + 1 < args.size())
        {
            output_file = args[i + 1];
            ++i;
        }
        else if (args[i] == ">>" && i + 1 < args.size())
        {
            output_file = args[i + 1];
            append = true;
            ++i;
        }
        else
        {
            cmd_args.push_back(args[i]);
        }
    }

    if (cmd_args.empty())
    {
        std::cerr << "No command to execute.\n";
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process

        // Input redirection
        if (!input_file.empty())
        {
            int fd = open(input_file.c_str(), O_RDONLY);
            if (fd < 0)
            {
                std::cerr << "Input file not found: " << input_file << std::endl;
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Output redirection
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
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Prepare execvp arguments
        std::vector<char *> exec_args;
        for (const auto &arg : cmd_args)
            exec_args.push_back(const_cast<char *>(arg.c_str()));
        exec_args.push_back(nullptr);

        execvp(exec_args[0], exec_args.data());
        perror("execvp failed");
        exit(127);
    }
    else if (pid > 0)
    {
        if (!background)
        {
            int status;
            waitpid(pid, &status, 0);
        }
        else
        {
            std::cout << "Process started in background with PID: " << pid << std::endl;
        }
    }
    else
    {
        perror("fork failed");
    }
}

// Execute a pipeline of commands separated by '|'
void Shell::executePipeline(const std::string &input)
{
    // Split by | respecting quotes by reusing the tokenisation.......
    std::vector<std::string> parts;
    std::string cur;
    bool in_quotes = false;
    char qc = '\0';
    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (!in_quotes && (c == '"' || c == '\''))
        {
            in_quotes = true;
            qc = c;
            cur.push_back(c);
            continue;
        }
        if (in_quotes && c == qc)
        {
            in_quotes = false;
            cur.push_back(c);
            continue;
        }
        if (!in_quotes && c == '|')
        {
            parts.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty())
        parts.push_back(cur);

    if (parts.empty())
        return;

    size_t n = parts.size();
    std::vector<int> fds(2 * (n - 1));
    for (size_t i = 0; i < n - 1; ++i)
    {
        if (pipe(&fds[2 * i]) == -1)
        {
            perror("pipe");
            return;
        }
    }

    auto trim = [](std::string &s)
    {
        // trim left
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                        { return !std::isspace(ch); }));
        // trim right
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                             { return !std::isspace(ch); })
                    .base(),
                s.end());
    };

    std::vector<pid_t> child_pids;
    for (size_t i = 0; i < n; ++i)
    {
        trim(parts[i]);
        {
            // use the shell's quote-aware tokenizer so quoted args (eg. awk scripts)
            // are passed without surrounding quotes
            std::vector<std::string> args = tokenize(parts[i]);

            if (args.empty())
                continue;

            // parse redirection tokens out of args for this segment
            std::vector<std::string> cmd_args;
            std::string input_file, output_file;
            bool append = false;
            for (size_t ai = 0; ai < args.size(); ++ai)
            {
                if (args[ai] == "<" && ai + 1 < args.size())
                {
                    input_file = args[ai + 1];
                    ++ai;
                }
                else if (args[ai] == ">" && ai + 1 < args.size())
                {
                    output_file = args[ai + 1];
                    ++ai;
                }
                else if (args[ai] == ">>" && ai + 1 < args.size())
                {
                    output_file = args[ai + 1];
                    append = true;
                    ++ai;
                }
                else
                {
                    cmd_args.push_back(args[ai]);
                }
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                perror("fork");
                return;
            }
            if (pid == 0)
            {
                // in child: setpgid to the group's pgid (first child's pid)
                if (i == 0)
                {
                    setpgid(0, 0); // first child becomes group leader
                }
                else
                {
                    // join first child's pgid which is stored in parent-side variable via wait/wiring
                    // using getppid() is not reliable; instead, setpgid to parent's knowledge via pipe isn't available here
                    // Best-effort: setpgid to the pid of the first child if parent exported it; fallback to setpgid(0,0)
                    setpgid(0, 0);
                }
                // set up stdin from previous pipe (if any)
                if (i > 0)
                {
                    dup2(fds[2 * (i - 1)], STDIN_FILENO);
                }
                // set up stdout to next pipe (if any)
                if (i < n - 1)
                {
                    dup2(fds[2 * i + 1], STDOUT_FILENO);
                }

                // now apply per-segment file redirections which override pipe fds
                if (!input_file.empty())
                {
                    int infd = open(input_file.c_str(), O_RDONLY);
                    if (infd < 0)
                    {
                        std::cerr << "Input file not found: " << input_file << std::endl;
                        _exit(1);
                    }
                    dup2(infd, STDIN_FILENO);
                    close(infd);
                }
                if (!output_file.empty())
                {
                    int outfd;
                    if (append)
                        outfd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                    else
                        outfd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (outfd < 0)
                    {
                        std::cerr << "Output file error: " << output_file << std::endl;
                        _exit(1);
                    }
                    dup2(outfd, STDOUT_FILENO);
                    close(outfd);
                }

                // close all pipe fds in child
                for (size_t j = 0; j < fds.size(); ++j)
                    close(fds[j]);

                // if nothing to run after stripping redirections, exit
                if (cmd_args.empty())
                    _exit(0);

                // build argv and exec
                std::vector<char *> exec_args;
                for (auto &a : cmd_args)
                    exec_args.push_back(const_cast<char *>(a.c_str()));
                exec_args.push_back(nullptr);

                execvp(exec_args[0], exec_args.data());
                perror("execvp");
                _exit(127);
            }
            // parent continues
            if (pid > 0)
            {
                child_pids.push_back(pid);
            }
        }
    }

    // parent: close fds
    for (size_t j = 0; j < fds.size(); ++j)
        close(fds[j]);

    if (!child_pids.empty())
    {
        pid_t pgid = child_pids[0];
        // set all children to the same pgid (join the group led by first child)
        for (size_t i = 0; i < child_pids.size(); ++i)
        {
            if (setpgid(child_pids[i], pgid) == -1)
            {
                // non-fatal; continue
            }
        }

        // set foreground pgid so signal handlers in shell can target the pipeline
        setForegroundPGID(pgid);

        // wait for all processes in the group; detect stopped jobs
        int status = 0;
        while (true)
        {
            pid_t w = waitpid(-pgid, &status, WUNTRACED);
            if (w == -1)
            {
                if (errno == ECHILD)
                    break;
                else
                    break;
            }
            if (WIFSTOPPED(status))
            {
                // move the job to background_jobs with the pipeline string as name
                background_jobs.push_back({pgid, input});
                std::cout << "\nProcess group " << pgid << " stopped and moved to background" << std::endl;
                break; // leave remaining children to be reaped later
            }
            // if exited/terminated, continue waiting until no children left
        }

        setForegroundPGID(0);
    }
}