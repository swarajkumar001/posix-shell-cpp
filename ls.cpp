#include "shell.h"
#include <iostream>
#include <vector> // For vector
#include <string>   // For string
#include <sys/types.h>  // For pid_t
#include <dirent.h>     // For DIR, struct dirent
#include <sys/stat.h>    // For struct stat
#include <pwd.h>         // For getpwuid
#include <grp.h>         // For getgrgid
#include <ctime>     // For time functions
#include <iomanip>    // For std::setw
#include <algorithm>  // For std::sort..

void printLongFormat(const std::string& full_path, const std::string& name) {
    struct stat file_stat;
    if (stat(full_path.c_str(), &file_stat) == -1) {
        perror(("ls: cannot access '" + name + "'").c_str());
        return;
    }
    // Permissions
    char perms[11];
    perms[0] = S_ISDIR(file_stat.st_mode) ? 'd' : '-';
    perms[1] = (file_stat.st_mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (file_stat.st_mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (file_stat.st_mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (file_stat.st_mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (file_stat.st_mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (file_stat.st_mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (file_stat.st_mode & S_IROTH) ? 'r' : '-';
    perms[8] = (file_stat.st_mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (file_stat.st_mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';

    // Links, owner, group
    int nlink = file_stat.st_nlink;
    struct passwd* pw = getpwuid(file_stat.st_uid);
    struct group* gr = getgrgid(file_stat.st_gid);
    std::string owner = pw ? pw->pw_name : std::to_string(file_stat.st_uid);
    std::string group = gr ? gr->gr_name : std::to_string(file_stat.st_gid);

    // Size
    off_t size = file_stat.st_size;

    // Modification time
    char timebuf[80];
    struct tm* tm_info = localtime(&file_stat.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);

    // Print info
    std::cout << perms << " "
              << std::setw(3) << nlink << " "
              << std::setw(8) << owner << " "
              << std::setw(8) << group << " "
              << std::setw(8) << size << " "
              << timebuf << " "
              << name << std::endl;
}

void listDirectory(const std::string& path, bool show_all, bool long_format) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        perror(("ls: cannot open directory '" + path + "'").c_str());
        return;
    }

    std::vector<std::string> names;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (!show_all && name[0] == '.') continue;
        names.push_back(name);
    }
    closedir(dir); //close the directory....
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
        std::string full_path = path + "/" + name;
        if (long_format) {
            printLongFormat(full_path, name);
        } else {
            std::cout << name << std::endl;
        }
    }
}

// Print a single file's info...........
void printFile(const std::string& path, bool long_format) {
    if (long_format) {
        std::string name = path;
        size_t pos = path.find_last_of('/');
        if (pos != std::string::npos) name = path.substr(pos+1);
        printLongFormat(path, name);
    } else {
        std::cout << path << std::endl;
    }
}



// Handle the 'ls' command with flags and paths
void handleLS(const std::vector<std::string>& args) {
    bool show_all = false, long_format = false;
    std::vector<std::string> paths;
    // Parse flags and directories/files
    for (size_t i = 1; i < args.size(); ++i) {
        std::string arg = args[i];
        if (!arg.empty() && arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                if (arg[j] == 'a') show_all = true;
                else if (arg[j] == 'l') long_format = true;
            }
        } else {
            paths.push_back(arg);
        }
    }

    // Default: current directory
    if (paths.empty()) paths.push_back(".");

    for (const std::string& raw_path : paths) {
        std::string path = raw_path;
        if (path == "~") path = getenv("HOME") ? getenv("HOME") : "/";
        struct stat path_stat;
        if (stat(path.c_str(), &path_stat) != 0) {
            perror(("ls: cannot access '" + path + "'").c_str());
            continue;
        }
        if (S_ISDIR(path_stat.st_mode)) {
            listDirectory(path, show_all, long_format);
        } else {
            printFile(path, long_format);
        }
    }
}

