// pinfo.cpp : process information read from /proc (no popen)
#include "shell.h"

#include <climits>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

static bool readFile(const std::string &path, std::string &out)
{
    out.clear();
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    char buf[4096];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, r);
    close(fd);
    return r == 0 && !out.empty();
}

// pinfo [pid] : status, virtual memory and executable path of a process
// (the shell itself when no pid is given).
void Shell::handlePinfo(const std::vector<std::string> &args)
{
    if (args.size() > 2)
    {
        std::cerr << "Usage: pinfo [pid]" << std::endl;
        return;
    }

    pid_t pid = getpid();
    if (args.size() == 2)
    {
        try
        {
            size_t pos = 0;
            long v = std::stol(args[1], &pos);
            if (pos != args[1].size() || v <= 0)
                throw 0;
            pid = static_cast<pid_t>(v);
        }
        catch (...)
        {
            std::cerr << "pinfo: invalid pid '" << args[1] << "'" << std::endl;
            return;
        }
    }

    const std::string proc = "/proc/" + std::to_string(pid);

    // /proc/<pid>/stat : "pid (comm) state ppid pgrp session tty_nr tpgid ..."
    std::string stat;
    if (!readFile(proc + "/stat", stat))
    {
        std::cerr << "pinfo: process " << pid << " does not exist" << std::endl;
        return;
    }
    size_t rpar = stat.rfind(')'); // comm may itself contain spaces or ')'
    if (rpar == std::string::npos || rpar + 2 > stat.size())
    {
        std::cerr << "pinfo: unexpected format in " << proc << "/stat" << std::endl;
        return;
    }
    std::istringstream ss(stat.substr(rpar + 2));
    char state = '?';
    long ppid = 0, pgrp = 0, session = 0, tty_nr = 0, tpgid = 0;
    ss >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid;

    std::string status(1, state);
    if (tty_nr != 0 && pgrp == tpgid) // process group owns the terminal => foreground
        status += '+';

    // /proc/<pid>/statm : first field is total virtual memory in pages
    unsigned long vm_kb = 0;
    std::string statm;
    if (readFile(proc + "/statm", statm))
    {
        std::istringstream ms(statm);
        unsigned long pages = 0;
        ms >> pages;
        long page_size = sysconf(_SC_PAGESIZE);
        vm_kb = pages * static_cast<unsigned long>(page_size > 0 ? page_size : 4096) / 1024;
    }

    char exe[PATH_MAX];
    ssize_t len = readlink((proc + "/exe").c_str(), exe, sizeof(exe) - 1);
    std::string exe_path = (len >= 0) ? displayPath(std::string(exe, len)) : "(permission denied)";

    std::cout << "pid -- " << pid << '\n'
              << "Process Status -- " << status << '\n'
              << "memory -- " << vm_kb << '\n'
              << "Executable Path -- " << exe_path << std::endl;
}
