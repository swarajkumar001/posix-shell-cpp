CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET   := main

SRCS := main.cpp shell.cpp display.cpp cd.cpp echo.cpp ls.cpp pinfo.cpp search.cpp \
        history.cpp io_pipe.cpp system_command.cpp bg_fg.cpp tab.cpp
OBJS := $(SRCS:.cpp=.o)
HDRS := shell.h bg_fg.h display.h tab.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	bash tests/smoke_test.sh

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all run test clean
