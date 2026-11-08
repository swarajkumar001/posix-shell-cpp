// ls.cpp : ls built-in with -a and -l (implemented with opendir/readdir/lstat, no exec)
#include "shell.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <grp.h>
#include <iostream>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{

struct Entry
{
    std::string name; // what is printed
    std::string path; // what is lstat'ed
    struct stat st;
};

std::string modeString(mode_t m)
{
    std::string s(10, '-');
    if (S_ISDIR(m))
        s[0] = 'd';
    else if (S_ISLNK(m))
        s[0] = 'l';
    else if (S_ISCHR(m))
        s[0] = 'c';
    else if (S_ISBLK(m))
        s[0] = 'b';
    else if (S_ISFIFO(m))
        s[0] = 'p';
    else if (S_ISSOCK(m))
        s[0] = 's';

    const char rwx[] = "rwxrwxrwx";
    const mode_t bits[9] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    for (int i = 0; i < 9; ++i)
        if (m & bits[i])
            s[i + 1] = rwx[i];

    if (m & S_ISUID)
        s[3] = (m & S_IXUSR) ? 's' : 'S';
    if (m & S_ISGID)
        s[6] = (m & S_IXGRP) ? 's' : 'S';
    if (m & S_ISVTX)
        s[9] = (m & S_IXOTH) ? 't' : 'T';
    return s;
}

std::string userName(uid_t uid)
{
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
}

std::string groupName(gid_t gid)
{
    struct group *gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
}

// Same rule as GNU ls: recent files show the time, older (or future) ones the year.
std::string timeString(time_t t)
{
    static const time_t six_months = 31556952 / 2;
    time_t now = time(nullptr);
    char buf[64];
    struct tm *tm = localtime(&t);
    if (!tm)
        return "?";
    if (t > now - six_months && t <= now)
        strftime(buf, sizeof(buf), "%b %e %H:%M", tm);
    else
        strftime(buf, sizeof(buf), "%b %e  %Y", tm);
    return buf;
}

std::string pad(const std::string &s, size_t width, bool left)
{
    if (s.size() >= width)
        return s;
    std::string fill(width - s.size(), ' ');
    return left ? s + fill : fill + s;
}

bool byName(const Entry &a, const Entry &b)
{
    return strcoll(a.name.c_str(), b.name.c_str()) < 0;
}

void printEntries(std::vector<Entry> &entries, bool long_format, bool show_total)
{
    std::sort(entries.begin(), entries.end(), byName);

    if (!long_format)
    {
        for (const auto &e : entries)
            std::cout << e.name << '\n';
        return;
    }

    struct Row
    {
        std::string mode, links, user, group, size, time, name;
    };
    std::vector<Row> rows;
    size_t w_links = 0, w_user = 0, w_group = 0, w_size = 0;
    long long blocks = 0; // st_blocks is in 512-byte units

    for (const auto &e : entries)
    {
        Row r;
        r.mode = modeString(e.st.st_mode);
        r.links = std::to_string(static_cast<unsigned long>(e.st.st_nlink));
        r.user = userName(e.st.st_uid);
        r.group = groupName(e.st.st_gid);
        r.size = std::to_string(static_cast<long long>(e.st.st_size));
        r.time = timeString(e.st.st_mtime);
        r.name = e.name;
        if (S_ISLNK(e.st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t n = readlink(e.path.c_str(), target, sizeof(target) - 1);
            if (n >= 0)
                r.name += " -> " + std::string(target, n);
        }
        w_links = std::max(w_links, r.links.size());
        w_user = std::max(w_user, r.user.size());
        w_group = std::max(w_group, r.group.size());
        w_size = std::max(w_size, r.size.size());
        blocks += e.st.st_blocks;
        rows.push_back(r);
    }

    if (show_total)
        std::cout << "total " << (blocks + 1) / 2 << '\n'; // in 1K blocks, like ls
    for (const auto &r : rows)
        std::cout << r.mode << ' '
                  << pad(r.links, w_links, false) << ' '
                  << pad(r.user, w_user, true) << ' '
                  << pad(r.group, w_group, true) << ' '
                  << pad(r.size, w_size, false) << ' '
                  << r.time << ' ' << r.name << '\n';
}

void listDirectory(const std::string &path, bool show_all, bool long_format)
{
    DIR *dp = opendir(path.c_str());
    if (!dp)
    {
        std::cerr << "ls: cannot open directory '" << path << "': " << strerror(errno) << std::endl;
        return;
    }

    std::vector<Entry> entries;
    struct dirent *d;
    while ((d = readdir(dp)) != nullptr)
    {
        Entry e{};
        e.name = d->d_name;
        if (!show_all && e.name[0] == '.')
            continue;
        e.path = path + "/" + e.name;
        if (lstat(e.path.c_str(), &e.st) != 0)
            continue;
        entries.push_back(e);
    }
    closedir(dp);
    printEntries(entries, long_format, true);
}

} // namespace

// ls [-a] [-l] [file/dir ...] : flags and names may appear in any order.
void Shell::handleLS(const std::vector<std::string> &args)
{
    bool show_all = false, long_format = false;
    std::vector<std::string> operands;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const std::string &a = args[i];
        if (a.size() > 1 && a[0] == '-')
        {
            for (size_t j = 1; j < a.size(); ++j)
            {
                if (a[j] == 'a')
                    show_all = true;
                else if (a[j] == 'l')
                    long_format = true;
                else
                {
                    std::cerr << "ls: invalid option -- '" << a[j] << "'" << std::endl;
                    return;
                }
            }
        }
        else
        {
            operands.push_back(expandHome(a));
        }
    }
    if (operands.empty())
        operands.push_back(".");

    // Like GNU ls: plain files first, then each directory's contents.
    std::vector<Entry> files;
    std::vector<std::string> dirs;
    for (const auto &path : operands)
    {
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
        {
            std::cerr << "ls: cannot access '" << path << "': " << strerror(errno) << std::endl;
            continue;
        }
        if (S_ISDIR(st.st_mode))
        {
            dirs.push_back(path);
        }
        else
        {
            Entry e{};
            e.name = path;
            e.path = path;
            if (lstat(path.c_str(), &e.st) != 0)
                e.st = st;
            files.push_back(e);
        }
    }
    std::sort(dirs.begin(), dirs.end(), [](const std::string &a, const std::string &b)
              { return strcoll(a.c_str(), b.c_str()) < 0; });

    bool headers = operands.size() > 1;
    bool first = true;
    if (!files.empty())
    {
        printEntries(files, long_format, false);
        first = false;
    }
    for (const auto &dir : dirs)
    {
        if (!first)
            std::cout << '\n';
        first = false;
        if (headers)
            std::cout << dir << ":\n";
        listDirectory(dir, show_all, long_format);
    }
    std::cout.flush();
}
