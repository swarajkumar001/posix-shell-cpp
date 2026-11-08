// io_pipe.cpp : I/O redirection (<, >, >>) and pipelines of any length
#include "shell.h"
#include "bg_fg.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

static bool isOperator(const std::string &t)
{
    return t == "<" || t == ">" || t == ">>" || t == "|" || t == "&";
}

// Separates redirection operators from the argument list of one stage.
bool Shell::parseCommand(const std::vector<std::string> &tokens, Command &cmd)
{
    cmd = Command();
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const std::string &t = tokens[i];
        if (t == "<" || t == ">" || t == ">>")
        {
            if (i + 1 >= tokens.size() || isOperator(tokens[i + 1]))
            {
                std::cerr << "syntax error near '" << t << "'" << std::endl;
                return false;
            }
            const std::string &file = tokens[++i];
            if (t == "<")
                cmd.in_file = file;
            else
            {
                cmd.out_file = file;
                cmd.append = (t == ">>");
            }
        }
        else if (t == "&")
        {
            std::cerr << "syntax error near '&'" << std::endl;
            return false;
        }
        else
        {
            cmd.argv.push_back(t);
        }
    }
    return true;
}

// Points stdin/stdout of the current process at the redirection files.
bool Shell::applyRedirection(const Command &cmd)
{
    if (!cmd.in_file.empty())
    {
        std::string path = expandHome(cmd.in_file);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            if (errno == ENOENT)
                std::cerr << cmd.in_file << ": input file does not exist" << std::endl;
            else
                std::cerr << cmd.in_file << ": " << strerror(errno) << std::endl;
            return false;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (!cmd.out_file.empty())
    {
        std::string path = expandHome(cmd.out_file);
        int flags = O_WRONLY | O_CREAT | (cmd.append ? O_APPEND : O_TRUNC);
        int fd = open(path.c_str(), flags, 0644);
        if (fd < 0)
        {
            std::cerr << cmd.out_file << ": " << strerror(errno) << std::endl;
            return false;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return true;
}

// Runs cmds[0] | cmds[1] | ... as one job (one process group). Every stage may
// have its own redirections, and built-ins work inside pipelines too.
void Shell::executePipeline(const std::vector<Command> &cmds, bool background, const std::string &name)
{
    const size_t n = cmds.size();
    pid_t pgid = 0;
    int prev_read = -1;
    std::cout.flush(); // don't duplicate buffered output into the children

    for (size_t i = 0; i < n; ++i)
    {
        int fd[2] = {-1, -1};
        if (i + 1 < n && pipe(fd) < 0)
        {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            if (fd[0] != -1)
            {
                close(fd[0]);
                close(fd[1]);
            }
            break;
        }

        if (pid == 0)
        {
            // Child: join the job's process group (the first stage leads it).
            setpgid(0, pgid);
            if (!background && interactive && i == 0)
                tcsetpgrp(STDIN_FILENO, getpid());
            resetChildSignals();

            if (prev_read != -1)
            {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1)
            {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                close(fd[0]);
            }

            // File redirections override the pipe ends.
            if (!applyRedirection(cmds[i]))
                _exit(1);

            if (isBuiltin(cmds[i].argv[0]))
            {
                runBuiltin(cmds[i].argv);
                std::cout.flush();
                _exit(0);
            }
            execExternal(cmds[i].argv);
        }

        // Parent
        if (pgid == 0)
            pgid = pid;
        setpgid(pid, pgid);
        if (prev_read != -1)
            close(prev_read);
        if (fd[1] != -1)
            close(fd[1]);
        prev_read = fd[0];
    }
    if (prev_read != -1)
        close(prev_read);
    if (pgid == 0)
        return;

    if (background)
    {
        jobs.push_back({pgid, name, false});
        std::cout << "[" << jobs.size() << "] " << pgid << std::endl;
    }
    else
    {
        waitForeground(pgid, name);
    }
}
