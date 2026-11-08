// bg_fg.cpp : signals (Ctrl-C, Ctrl-Z, SIGCHLD) and job control (jobs, fg, bg)
#include "shell.h"
#include "bg_fg.h"

#include <cerrno>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t fg_pgid = 0;
volatile sig_atomic_t child_changed = 0;

// Ctrl-C: interrupt the foreground job. The shell itself is never killed.
static void handle_sigint(int)
{
    if (fg_pgid > 0)
        kill(-fg_pgid, SIGINT);
}

// Ctrl-Z: stop the foreground job. The shell itself is never stopped.
static void handle_sigtstp(int)
{
    if (fg_pgid > 0)
        kill(-fg_pgid, SIGTSTP);
}

// A child exited or stopped: let the main loop reap background jobs.
static void handle_sigchld(int)
{
    child_changed = 1;
}

void setupShellSignals()
{
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, nullptr);
    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, nullptr);
    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, nullptr);

    // The shell gives the terminal to foreground jobs and takes it back with
    // tcsetpgrp(), which raises SIGTTOU when called from a background group.
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

void resetChildSignals()
{
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

// Gives the terminal to job `pgid` and waits until every process in it has
// finished or the job is stopped with Ctrl-Z (it is then added to `jobs`).
void Shell::waitForeground(pid_t pgid, const std::string &name, bool resume)
{
    fg_pgid = pgid;
    if (interactive)
        tcsetpgrp(STDIN_FILENO, pgid);
    if (resume)
        kill(-pgid, SIGCONT);

    bool stopped = false;
    bool interrupted = false;
    int status = 0;
    while (true)
    {
        pid_t w = waitpid(-pgid, &status, WUNTRACED);
        if (w == -1)
        {
            if (errno == EINTR)
                continue;
            break; // ECHILD: all processes of the job are done
        }
        if (WIFSTOPPED(status))
        {
            stopped = true;
            break;
        }
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
            interrupted = true;
    }

    if (interactive)
    {
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        tcsetattr(STDIN_FILENO, TCSADRAIN, &orig_termios);
    }
    fg_pgid = 0;

    if (stopped)
    {
        jobs.push_back({pgid, name, true});
        std::cout << "\n[" << jobs.size() << "] Stopped  " << name << " (pid " << pgid << ")" << std::endl;
    }
    else if (interrupted)
    {
        std::cout << std::endl;
    }
}

// Reaps background jobs that have finished and reports them.
void Shell::reapBackgroundJobs()
{
    for (size_t i = 0; i < jobs.size();)
    {
        Job &job = jobs[i];
        int status = 0, last = 0;
        bool reaped = false;
        pid_t r;
        while ((r = waitpid(-job.pgid, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
        {
            if (WIFSTOPPED(status))
                job.stopped = true;
            else if (WIFCONTINUED(status))
                job.stopped = false;
            else
            {
                last = status;
                reaped = true;
            }
        }
        if (r == -1 && errno == ECHILD)
        {
            bool normal = !reaped || (WIFEXITED(last) && WEXITSTATUS(last) == 0);
            std::cout << job.name << " with pid " << job.pgid << " exited "
                      << (normal ? "normally" : "abnormally") << std::endl;
            jobs.erase(jobs.begin() + i);
            continue;
        }
        ++i;
    }
}

// Returns the index of the job named by args[1] (a pid), or the most recent job.
int Shell::findJob(const std::vector<std::string> &args, const char *cmd)
{
    if (jobs.empty())
    {
        std::cerr << cmd << ": no current job" << std::endl;
        return -1;
    }
    if (args.size() < 2)
        return static_cast<int>(jobs.size()) - 1;

    try
    {
        pid_t pid = static_cast<pid_t>(std::stol(args[1]));
        for (size_t i = 0; i < jobs.size(); ++i)
            if (jobs[i].pgid == pid)
                return static_cast<int>(i);
    }
    catch (...)
    {
    }
    std::cerr << cmd << ": " << args[1] << ": no such job" << std::endl;
    return -1;
}

void Shell::handleJobs()
{
    for (size_t i = 0; i < jobs.size(); ++i)
        std::cout << "[" << i + 1 << "] " << (jobs[i].stopped ? "Stopped " : "Running ")
                  << jobs[i].name << " (pid " << jobs[i].pgid << ")" << std::endl;
}

void Shell::handleFg(const std::vector<std::string> &args)
{
    int idx = findJob(args, "fg");
    if (idx < 0)
        return;
    Job job = jobs[idx];
    jobs.erase(jobs.begin() + idx);
    std::cout << job.name << std::endl;
    waitForeground(job.pgid, job.name, true);
}

void Shell::handleBg(const std::vector<std::string> &args)
{
    int idx = findJob(args, "bg");
    if (idx < 0)
        return;
    Job &job = jobs[idx];
    if (!job.stopped)
    {
        std::cerr << "bg: job " << job.pgid << " is already running" << std::endl;
        return;
    }
    kill(-job.pgid, SIGCONT);
    job.stopped = false;
    std::cout << "[" << idx + 1 << "] " << job.name << " &" << std::endl;
}
