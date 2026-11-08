#include "shell.h"
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>


// Foreground process group id (should be set by shell before running a foreground job)
volatile sig_atomic_t fg_pgid = 0;

// SIGINT handler (Ctrl-C)
void handle_sigint(int)
{
    if (fg_pgid > 0)
    {
        kill(-fg_pgid, SIGINT); // Send SIGINT to foreground group
    }
}

// SIGTSTP handler (Ctrl-Z)
void handle_sigtstp(int)
{
    if (fg_pgid > 0)
    {
        kill(-fg_pgid, SIGTSTP); // Send SIGTSTP to foreground group
        // Optionally: mark job as stopped in your Shell class
    }
}

// Setup signal handlers (call from Shell::run)
void setup_signal_handlers()
{
    struct sigaction sa_int = {};
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, nullptr);

    struct sigaction sa_tstp = {};
    sa_tstp.sa_handler = handle_sigtstp;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, nullptr);
}

volatile pid_t fg_pid = -1;

// Called for SIGINT (CTRL-C)
void sigint_handler(int signo)
{
    if (fg_pid > 0)
    {
        kill(fg_pid, SIGINT); // Forward SIGINT to fg child
    }
    // else: do nothing (don't kill shell)
}

// Called for SIGTSTP (CTRL-Z)
void sigtstp_handler(int signo)
{
    if (fg_pid > 0)
    {
        kill(fg_pid, SIGTSTP); // Suspend fg child
        // Optionally: mark as stopped in jobs table if you do job control
    }
    // else: do nothing
}

// Optionally, handle SIGCHLD for reaping background/zombie children
void sigchld_handler(int signo)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
}
