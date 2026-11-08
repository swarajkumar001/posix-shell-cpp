#include <iostream>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

void executeSystemCommand(const std::vector<std::string>& args, bool background) {
    if (args.empty()) return;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // Child process
        std::vector<char*> exec_args;// Convert args to char* array
        for (const auto& arg : args) {
            exec_args.push_back(const_cast<char*>(arg.c_str()));
        }
        exec_args.push_back(nullptr); // Null-terminate the array

        if (execvp(exec_args[0], exec_args.data()) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        if (background) {
            std::cout << "Process started in background with PID: " << pid << std::endl;
            // Add tracking of background processes here, if desired
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}
