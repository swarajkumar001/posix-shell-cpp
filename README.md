Name :- swaraj kumar

## Introduction  
This project is an implementation of a custom **POSIX-like shell** in **C++**as part of Advanced Operating Systems (Monsoon 2025).  
The shell supports built-in commands, execution of system commands in foreground/background, I/O redirection, pipelines, signals, history, and autocomplete.  

The code is modular, with each built-in command implemented in a separate file.  

---

## Features Implemented  

### Shell Prompt and Input Parsing  
- Displays prompt in format:  

- Uses system calls:  
- `getenv()` → username  
- `gethostname()` → hostname  
- `getcwd()` → current directory  
- Input read using `getchar()`.  
- Tokenization done using `strtok()` with delimiters `;`, `|`, whitespace.  
- Supports multiple commands in one line and background execution using `&`.  

---

### Built-in Commands  

**cd [dir]**  
- System calls: `chdir()`, `struct stat`.  
- Tracks current path and previous path (`pth`, `prev_pth`).  
- Supports `.`, `..`, `-`, `~`.  
- Shows error for invalid/multiple arguments.  

**pwd**  
- Prints current directory using `getcwd()`.  

**echo [args...]**  
Prints given arguments.  
Handles multiple spaces/tabs.  

**ls [-a] [-l] [paths...]**  
- Uses `struct stat` to fetch file info.  
- Uses `opendir()`, `readdir()` to list files.  
- Flags: `-a`, `-l`, and combinations.  
- Handles multiple directories.  

**pinfo [pid]**  
- Reads from `/proc/<pid>/`.  
- Shows: process status, virtual memory, and executable path.  
- Marks foreground process with `+`.  

**search <filename/dir>**  
- Recursively searches directories.  
- Uses `struct stat`, `opendir()`, and `readdir()`.  
- Returns `True` if found, else `False`.  

**history [n]**  
- Stores up to 20 commands across sessions.  
- Default: show last 10 commands.  
- Supports `history n` for last `n` commands.  
- Implemented with a `deque`.  
- Up/Down arrow navigation supported.  

**Other built-ins**  
- `clear` → clears terminal.  
- `exit` → exits shell.  

---

### System Commands and Process Handling  
- External commands executed with `fork()`, `execvp()`, and `waitpid()`.  
- Foreground processes: shell waits for completion.  
- Background processes: detected with `&`, run in background, PID displayed.  
- Multiple background jobs supported simultaneously.  

---

### I/O Redirection and Pipelines  
- **Redirection:**  
- `< file` → input redirection.  
- `> file` → output overwrite.  
- `>> file` → output append.  
- Uses `dup2()` to replace `stdin` / `stdout`.  
- **Pipelines:**  
- Tokenized using `|`.  
- Each command executed with `pipe()`, `dup2()`, `fork()`.  
- Supports multiple pipes chained together.  
- Works with redirection inside pipelines.  

---

### Signal Handling  
- Implemented with custom handlers.  
- `CTRL+C` → sends `SIGINT` to foreground job.  
- `CTRL+Z` → stops foreground job and pushes to background.  
- `CTRL+D` → exits the shell.  
- Shell itself is not terminated accidentally.  

---

### Autocomplete  
- Implemented for commands and files/directories.  
- Triggered by `TAB`.  
- Single match → auto-completes.  
- Multiple matches → prints all options.  

---


## File Mapping  

-
> main.cpp → Program entry point
> display.cpp → Shell prompt display
> cd.cpp → Change directory command
> echo.cpp → Echo command handler
> ls.cpp → List directory contents
> pinfo.cpp → Process info fetcher
> search.cpp → Recursive file search
> history.cpp → Command history manager
> io_pipe.cpp → Piping & I/O redirection
> tab.cpp → Autocomplete utilities
> bg_fg.cpp → Foreground & background signal handling
> Makefile → Build, clean, and run targets
> 
---

## Steps to Run  

1. make clean
  2. make
    3 ./main

     you will enter in my builtin shell and test the built command...
