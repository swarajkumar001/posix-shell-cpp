
main: main.cpp cd.cpp echo.cpp ls.cpp  system_command.cpp  pinfo.cpp  shell.cpp search.cpp io_pipe.cpp display.cpp bg_fg.cpp history.cpp tab.cpp
	g++ -std=c++17 -Wall -g main.cpp cd.cpp echo.cpp ls.cpp  system_command.cpp  pinfo.cpp  shell.cpp search.cpp io_pipe.cpp display.cpp bg_fg.cpp history.cpp tab.cpp -o main

clean:
	rm -f main *.o

run: main
	./main

.PHONY: clean run



