#include <iostream>
#include <pwd.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>


// Function to display the shell prompt..........
void displayPrompt() {
    // Get username
    const char* username = getenv("USER");  // Fallback to getpwuid if USER not set.......
    if (!username) {
        struct passwd* pw = getpwuid(getuid());
        username = pw ? pw->pw_name : "unknown";
    }


    // Get system name
    struct utsname sysinfo;
    uname(&sysinfo);
    std::string systemname = sysinfo.nodename;

    // Get current directory
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::string dir = cwd;

    // Replace home directory with ~
    const char* home = getenv("HOME");
    if (home && dir.find(home) == 0) {
        dir.replace(0, std::string(home).length(), "~");
    }


    std::cout << username << "@" << systemname << ":" << dir << "~ > ";  // Display prompt...
    std::cout.flush();
}