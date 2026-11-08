#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <sys/types.h>
#include <utility>

namespace Utils {
    // String manipulation utilities
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);
    
    // Path utilities
    std::string expandTilde(const std::string& path);
    std::string getCurrentDirectory();
    bool directoryExists(const std::string& path);
    
    // Process utilities
    bool isProcessRunning(pid_t pid);
    void cleanupFinishedJobs(std::vector<std::pair<pid_t, std::string>>& jobs);
    
    // Display utilities
    void printError(const std::string& message);
    void printSuccess(const std::string& message);
}

#endif // UTILS_H
