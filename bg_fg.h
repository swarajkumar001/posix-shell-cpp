#ifndef BG_FG_H
#define BG_FG_H

#include <signal.h>

// Process group of the job currently running in the foreground (0 = none).
extern volatile sig_atomic_t fg_pgid;

// Set by the SIGCHLD handler; the main loop then reaps finished background jobs.
extern volatile sig_atomic_t child_changed;

// Installs the shell's handlers for SIGINT, SIGTSTP and SIGCHLD.
void setupShellSignals();

// Restores default signal dispositions in a freshly forked child.
void resetChildSignals();

#endif
