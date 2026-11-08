# POSIX Shell in C++

An interactive Unix shell written from scratch in C++ using only POSIX system calls.
It has no `system()`, `popen()`, ncurses or `<filesystem>`, and `ls` isn't run through `exec`.
It was built for the Advanced Operating Systems course (Monsoon 2025).

```
swaraj@ubuntu:~> ls -la src | grep .cpp > files.txt ; cat < files.txt | wc -l
12
swaraj@ubuntu:~> sleep 30 &
[1] 48213
swaraj@ubuntu:~> vim notes.txt          # Ctrl-Z
[2] Stopped  vim notes.txt (pid 48220)
swaraj@ubuntu:~> fg 48220
```

## Features

| Area | What it does |
|------|--------------|
| **Prompt** | `user@host:dir>`. The directory the shell starts in is `~`. Username comes from `pwd.h` and the host name from `uname()`. |
| **Parsing** | `;`-separated command lists (split with `strtok`), any amount of spaces and tabs, quotes, and operators with or without spaces (`echo hi>out.txt`). |
| **Built-ins** | `cd` (`.` `..` `-` `~`), `pwd`, `echo`, `ls`, `pinfo`, `search`, `history`, `jobs`, `fg`, `bg`, `clear`, `exit` |
| **ls** | `-a`, `-l` and combined forms (`-la`, `-al`, `-l dir -a`), with any number of files or directories. `-l` output matches GNU `ls`: permissions incl. setuid/sticky bits, link count, owner, group, size, mtime, `total`, and `->` for symlinks. |
| **Processes** | Runs any program with `fork` + `execvp`, in the foreground or in the background with `&` (prints the PID). Each job gets its own process group and the terminal is handed over with `tcsetpgrp`. |
| **Redirection** | `<`, `>`, `>>`. New files get permissions 0644, and the shell reports an error if the input file is missing. |
| **Pipes** | Pipelines of any length (`a \| b \| c \| ...`), with redirection on any stage. Built-ins also work inside pipelines. |
| **Signals** | Ctrl-C interrupts the foreground job, Ctrl-Z stops it and moves it to the job list, and Ctrl-D logs out. The shell itself is never killed or stopped. A `SIGCHLD` handler is used to reap finished background jobs. |
| **pinfo** | Process state (`R/S/Z/T`, with `+` for foreground), virtual memory and executable path, read from `/proc/<pid>/{stat,statm,exe}`. |
| **search** | Recursive search for a file or folder name below the current directory, using `opendir`/`readdir`/`lstat`. Prints `True`/`False`. |
| **History** | Keeps 20 commands across sessions (in `~/.my_shell_history`). `history` shows 10 and `history n` shows n. UP/DOWN arrows browse history in a raw-mode line editor. |
| **Autocomplete** | TAB completes commands (built-ins and `$PATH`) and files in the current directory. A single match is completed, several matches fill in their common prefix, and otherwise all matches are listed. |

## Build and run (Linux)

```bash
make          # builds ./main
./main        # start the shell
make test     # run the automated tests
make clean
```

You need `g++` with C++17 and GNU `make`. This is a Linux shell: it uses `/proc` and POSIX terminal job control, so it won't build on native Windows. Use WSL or a Linux machine.

## Code layout

| File | Responsibility |
|------|----------------|
| `main.cpp` | Entry point |
| `shell.h` / `shell.cpp` | `Shell` class: main loop, raw-mode line editor (arrows, backspace, Ctrl-D), tokenizer, `;` splitting, built-in dispatch |
| `display.cpp` | Prompt |
| `cd.cpp` | `cd`, `pwd` |
| `echo.cpp` | `echo` |
| `ls.cpp` | `ls -a -l` |
| `pinfo.cpp` | `pinfo` |
| `search.cpp` | `search` |
| `history.cpp` | Persistent history and `history [n]` |
| `io_pipe.cpp` | Redirection parsing and the pipeline executor |
| `system_command.cpp` | `execvp` of external programs |
| `bg_fg.cpp` | Signal handlers, waiting for foreground jobs, reaping background jobs, `jobs`/`fg`/`bg` |
| `tab.cpp` | TAB completion |
| `tests/smoke_test.sh` | Automated tests that pipe commands into the shell and compare the output (`ls` output is compared with the system `ls`) |

## How it works

- **One job = one process group.** Every stage of a pipeline joins the process group of the first stage. For foreground jobs the shell gives that group the terminal (`tcsetpgrp`), so Ctrl-C and Ctrl-Z from the keyboard go only to the job. The shell then waits with `waitpid(-pgid, WUNTRACED)`, which also notices stopped jobs.
- **Built-ins** run inside the shell process so that `cd`, `exit` and `history` change the shell's own state. Redirection is applied with `dup`/`dup2` and undone afterwards. Inside a pipeline, a built-in runs in the forked child like any other stage.
- **Line editing** puts the terminal in non-canonical mode (`termios`, no ncurses) while reading a command and restores it before running anything.
