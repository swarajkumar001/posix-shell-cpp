#include "shell.h"
#include <unistd.h>     // For getpid, readlink
#include <sys/types.h>  // For pid_t
#include <sys/stat.h>   // For struct stat
#include <limits.h>
#include <fcntl.h>      
#include <string>
#include <sstream>
#include <iostream>  // For std::cout, std::cerr


//pinfo command implementation....
void Shell::handlePinfo(const std::vector<std::string> &args)
{
    // If no PID is given, use the current process id...
    pid_t qpid;
    if (args.size() == 1)
    {
        qpid = getpid();
    }
    else if (args.size() == 2) // pid provided as argument.........
    {
        try
        {
            qpid = static_cast<pid_t>(std::stol(args[1]));
        }
        catch (...)
        {
            std::cerr << "Invalid PID: " << args[1] << std::endl;
            return;
        }
    }
    else
    {
        std::cerr << "Usage: pinfo [pid]" << std::endl;
        return;
    }

    const std::string procDir = "/proc/" + std::to_string(qpid);  // Path to /proc/[pid] directory.....

    // helper: read entire file into a string using POSIX open/read
    auto readFileToString = [](const std::string &path, std::string &out) -> bool
    {
        out.clear();
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0)
            return false;
        char buf[4096];
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf))) > 0)
            out.append(buf, buf + r);
        close(fd);
        return (r == 0);
    };

    std::string line;
    if (!readFileToString(procDir + "/stat", line))
    {
        std::cerr << "Error: unable to open " << procDir << "/stat" << std::endl;
        return;
    }
    // stat contains a trailing newline; only keep up to the first newline
    size_t nl = line.find('\n');
    if (nl != std::string::npos)
        line.resize(nl);
    if (line.empty())
    {
        std::cerr << "Error: empty stat for " << qpid << std::endl;
        return;
    }

    // Extract pid, (comm), then rest
    size_t lpar = line.find('(');
    size_t rpar = line.rfind(')');
    if (lpar == std::string::npos || rpar == std::string::npos || rpar < lpar)
    {
        std::cerr << "Error: malformed stat for " << qpid << std::endl;
        return;
    }
    pid_t spid = 0;
    try
    {
        spid = static_cast<pid_t>(std::stol(line.substr(0, lpar)));
    }
    catch (...)
    {
        std::cerr << "Error: malformed pid in stat" << std::endl;
        return;
    }
    // std::string comm = line.substr(lpar + 1, rpar - lpar - 1); // unused
    std::string rest = line.substr(rpar + 2); // skip ") "

    std::istringstream ss(rest);
    char state = '?';
    long ppid = 0, pgrp = 0, session = 0, tty_nr = 0, tpgid = 0;
    ss >> state;   // process state
    ss >> ppid;    // parent process id
    ss >> pgrp;    // process group id
    ss >> session; // session id
    ss >> tty_nr;  // controlling terminal
    ss >> tpgid;   // terminal process group id.....

    std::string processStatus(1, state);
    if (pgrp == tpgid && tty_nr != 0)
        processStatus += "+";

    // Read memory from /proc/[pid]/statm
    std::string statmContent;
    if (!readFileToString(procDir + "/statm", statmContent))
    {
        std::cerr << "Error: unable to open " << procDir << "/statm" << std::endl;
        return;
    }

    // statm contains a trailing newline; only keep up to the first newline........
    std::istringstream msm(statmContent);
    unsigned long virtPages = 0;
    msm >> virtPages;
    long pageSize = sysconf(_SC_PAGESIZE);
    unsigned long virtKB = (pageSize > 0) ? (virtPages * (unsigned long)pageSize) / 1024UL : 0UL;

    // Executable path
    char exePath[PATH_MAX];
    ssize_t len = readlink((procDir + "/exe").c_str(), exePath, sizeof(exePath) - 1);
    if (len < 0)
    {
        std::cerr << "Error: unable to read executable path" << std::endl;
        return;
    }
    exePath[len] = '\0';

    std::cout << "pid -- " << spid << std::endl
              << "Process Status -- " << processStatus << std::endl
              << "memory -- " << virtKB << " KB" << std::endl
              << "Executable Path -- " << exePath << std::endl;
}
