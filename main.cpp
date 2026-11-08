#include "shell.h"
#include <iostream>

using namespace std;


//main entry point of the program...........
int main() {
    Shell shell;
    int status = shell.run();   
    if (status != 0) {
        cerr << "Shell error: code " << status << endl;
        return 1;
    }

    return 0;
}
