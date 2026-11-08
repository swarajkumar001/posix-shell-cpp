// shell.cpp : main loop, raw-mode line editor, tokenizer and command dispatch
#include "shell.h"
#include "bg_fg.h"
#include "display.h"

#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <pwd.h>
#include <termios.h>
#include <unistd.h>

const std::vector<std::string> BUILTINS = {
    "cd", "pwd", "echo", "ls", "pinfo", "search", "history",
    "jobs", "fg", "bg", "clear", "exit"};

Shell::Shell()
    : running(true), interactive(false), shell_pgid(0), orig_fg_pgid(0), orig_termios()
{
    // The directory the shell is invoked from is its home ("~").
    char cwd[PATH_MAX];
    home_dir = getcwd(cwd, sizeof(cwd)) ? cwd : "/";

    const char *user_home = getenv("HOME");
    if (!user_home)
    {
        struct passwd *pw = getpwuid(getuid());
        user_home = pw ? pw->pw_dir : nullptr;
    }
    history_file = std::string(user_home ? user_home : home_dir.c_str()) + "/.my_shell_history";
}

int Shell::run()
{
    setlocale(LC_COLLATE, ""); // so ls sorts names like the system ls
    interactive = isatty(STDIN_FILENO);
    setupShellSignals();

    if (interactive)
    {
        // Put the shell in its own process group and take control of the terminal.
        orig_fg_pgid = tcgetpgrp(STDIN_FILENO);
        setpgid(0, 0);
        shell_pgid = getpgrp();
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        tcgetattr(STDIN_FILENO, &orig_termios);
    }

    loadHistory();

    std::string line;
    while (running)
    {
        if (child_changed)
        {
            child_changed = 0;
            reapBackgroundJobs();
        }

        if (interactive)
        {
            printPrompt();
            enableRawMode();
        }
        bool ok = readLine(line);
        if (interactive)
            disableRawMode();

        if (!ok) // Ctrl-D or end of input
            break;
        if (line.find_first_not_of(" \t") == std::string::npos)
            continue;

        addToHistory(line);
        executeLine(line);
    }

    cleanup();
    return 0;
}

// ---------------- prompt and terminal ----------------

void Shell::printPrompt()
{
    char cwd[PATH_MAX];
    displayPrompt(getcwd(cwd, sizeof(cwd)) ? displayPath(cwd) : "?");
}

void Shell::enableRawMode()
{
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); // ISIG stays on so Ctrl-C / Ctrl-Z raise signals
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void Shell::disableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void Shell::redrawLine(const std::string &buf)
{
    std::cout << "\r\033[K";
    printPrompt();
    std::cout << buf << std::flush;
}

// Reads one line. Handles backspace, TAB, UP/DOWN history and Ctrl-D.
// Returns false on Ctrl-D / end of input.
bool Shell::readLine(std::string &buf)
{
    buf.clear();
    size_t hist_idx = history.size();
    std::string draft; // what the user had typed before browsing history
    char c;

    while (true)
    {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (n == 0) // EOF: run whatever is pending, then stop
            return !buf.empty();

        if (c == '\n' || c == '\r')
        {
            if (interactive)
                std::cout << std::endl;
            return true;
        }

        if (!interactive) // input from a pipe/file: no line editing
        {
            buf += c;
            continue;
        }

        if (c == 4) // Ctrl-D: log out
        {
            std::cout << std::endl;
            buf.clear();
            return false;
        }
        else if (c == 127 || c == '\b') // Backspace
        {
            if (!buf.empty())
            {
                buf.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        }
        else if (c == '\t')
        {
            handleTab(buf);
        }
        else if (c == 27) // escape sequence: arrow keys
        {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1)
                continue;
            if (seq[0] != '[')
                continue;
            if (seq[1] >= '0' && seq[1] <= '9') // e.g. Delete = ESC [ 3 ~ : ignore
            {
                char tail;
                if (read(STDIN_FILENO, &tail, 1) != 1)
                    continue;
                continue;
            }
            if (seq[1] == 'A') // UP: older command, stop at the oldest
            {
                if (hist_idx > 0)
                {
                    if (hist_idx == history.size())
                        draft = buf;
                    buf = history[--hist_idx];
                    redrawLine(buf);
                }
            }
            else if (seq[1] == 'B') // DOWN: newer command, only after UP
            {
                if (hist_idx < history.size())
                {
                    ++hist_idx;
                    buf = (hist_idx == history.size()) ? draft : history[hist_idx];
                    redrawLine(buf);
                }
            }
            // LEFT / RIGHT are ignored
        }
        else if (static_cast<unsigned char>(c) >= 32)
        {
            buf += c;
            std::cout << c << std::flush;
        }
    }
}

// ---------------- parsing and dispatch ----------------

// Splits a line into ';' separated commands using strtok.
void Shell::executeLine(const std::string &line)
{
    std::vector<char> buf(line.begin(), line.end());
    buf.push_back('\0');

    std::vector<std::string> parts;
    for (char *tok = strtok(buf.data(), ";"); tok != nullptr; tok = strtok(nullptr, ";"))
        parts.push_back(tok);

    for (const auto &part : parts)
    {
        if (!running)
            break;
        executeCommand(part);
    }
}

// Splits on spaces/tabs. |, <, >, >> and & become separate tokens even without
// surrounding spaces. Quotes group words and are removed.
std::vector<std::string> Shell::tokenize(const std::string &input)
{
    std::vector<std::string> out;
    std::string cur;
    bool have_token = false;
    char quote = '\0';

    auto flush = [&]()
    {
        if (have_token)
        {
            out.push_back(cur);
            cur.clear();
            have_token = false;
        }
    };

    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (quote)
        {
            if (c == quote)
                quote = '\0';
            else
                cur += c;
            continue;
        }
        if (c == '"' || c == '\'')
        {
            quote = c;
            have_token = true;
        }
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            flush();
        }
        else if (c == '|' || c == '<' || c == '&')
        {
            flush();
            out.push_back(std::string(1, c));
        }
        else if (c == '>')
        {
            flush();
            if (i + 1 < input.size() && input[i + 1] == '>')
            {
                out.push_back(">>");
                ++i;
            }
            else
                out.push_back(">");
        }
        else
        {
            cur += c;
            have_token = true;
        }
    }
    flush();
    return out;
}

void Shell::executeCommand(const std::string &cmdline)
{
    std::vector<std::string> tokens = tokenize(cmdline);
    if (tokens.empty())
        return;

    bool background = false;
    if (tokens.back() == "&")
    {
        background = true;
        tokens.pop_back();
        if (tokens.empty())
        {
            std::cerr << "syntax error near '&'" << std::endl;
            return;
        }
    }

    // Split into pipeline stages on '|'
    std::vector<Command> stages;
    std::vector<std::string> cur;
    for (size_t i = 0; i <= tokens.size(); ++i)
    {
        if (i == tokens.size() || tokens[i] == "|")
        {
            Command cmd;
            if (!parseCommand(cur, cmd))
                return;
            if (cmd.argv.empty())
            {
                std::cerr << "syntax error: missing command" << std::endl;
                return;
            }
            stages.push_back(cmd);
            cur.clear();
        }
        else
        {
            cur.push_back(tokens[i]);
        }
    }

    std::string name;
    for (const auto &t : tokens)
        name += (name.empty() ? "" : " ") + t;

    // A lone built-in runs inside the shell so cd/exit/history affect the shell
    // itself. Redirection is applied temporarily and then undone.
    if (stages.size() == 1 && isBuiltin(stages[0].argv[0]))
    {
        std::cout.flush();
        int saved_in = dup(STDIN_FILENO);
        int saved_out = dup(STDOUT_FILENO);
        if (applyRedirection(stages[0]))
            runBuiltin(stages[0].argv);
        std::cout.flush();
        dup2(saved_in, STDIN_FILENO);
        dup2(saved_out, STDOUT_FILENO);
        close(saved_in);
        close(saved_out);
        return;
    }

    executePipeline(stages, background, name);
}

bool Shell::isBuiltin(const std::string &name) const
{
    for (const auto &b : BUILTINS)
        if (b == name)
            return true;
    return false;
}

void Shell::runBuiltin(const std::vector<std::string> &args)
{
    const std::string &cmd = args[0];
    if (cmd == "cd")
        handleCD(args);
    else if (cmd == "pwd")
        handlePWD();
    else if (cmd == "echo")
        handleEcho(args);
    else if (cmd == "ls")
        handleLS(args);
    else if (cmd == "pinfo")
        handlePinfo(args);
    else if (cmd == "search")
        handleSearch(args);
    else if (cmd == "history")
        handleHistory(args);
    else if (cmd == "jobs")
        handleJobs();
    else if (cmd == "fg")
        handleFg(args);
    else if (cmd == "bg")
        handleBg(args);
    else if (cmd == "clear")
        std::cout << "\033[H\033[2J" << std::flush;
    else if (cmd == "exit")
        running = false;
}

// ---------------- helpers ----------------

// "~" and "~/x" refer to the shell's home (the directory it was started from).
std::string Shell::expandHome(const std::string &path) const
{
    if (path == "~")
        return home_dir;
    if (path.size() > 1 && path[0] == '~' && path[1] == '/')
        return home_dir + path.substr(1);
    return path;
}

// Inverse of expandHome, used for the prompt and pinfo.
std::string Shell::displayPath(const std::string &path) const
{
    if (path == home_dir)
        return "~";
    if (home_dir != "/" && path.compare(0, home_dir.size(), home_dir) == 0 &&
        path.size() > home_dir.size() && path[home_dir.size()] == '/')
        return "~" + path.substr(home_dir.size());
    return path;
}

void Shell::cleanup()
{
    // Stopped jobs would otherwise stay suspended forever.
    for (const auto &job : jobs)
    {
        if (job.stopped)
        {
            kill(-job.pgid, SIGHUP);
            kill(-job.pgid, SIGCONT);
        }
    }
    if (interactive)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        if (orig_fg_pgid > 0)
            tcsetpgrp(STDIN_FILENO, orig_fg_pgid);
    }
}
