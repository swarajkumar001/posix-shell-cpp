// display.cpp : shell prompt
#include "display.h"

#include <cstdlib>
#include <iostream>
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>

void displayPrompt(const std::string &dir)
{
    // Username from the password database (pwd.h), falling back to $USER.
    const char *user = nullptr;
    struct passwd *pw = getpwuid(getuid());
    if (pw)
        user = pw->pw_name;
    if (!user)
        user = getenv("USER");
    if (!user)
        user = "user";

    // System name from uname().
    struct utsname info;
    std::string host = (uname(&info) == 0) ? info.nodename : "localhost";

    std::cout << user << "@" << host << ":" << dir << "> " << std::flush;
}
