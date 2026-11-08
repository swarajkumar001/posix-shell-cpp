#ifndef BG_FG_H // Prevent multiple inclusions
#define BG_FG_H

#include <signal.h>

// Declaration of the fg_pgid variable to track the foreground process group ID..........
extern volatile sig_atomic_t fg_pgid;
void handle_sigint(int);
void handle_sigtstp(int);
void setup_signal_handlers();

#endif