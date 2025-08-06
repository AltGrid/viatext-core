# ====== Makefile for ViaText-Core ======

CXX = g++
CXXFLAGS = -std=c++17 \
    -Iinclude \
    -Iinclude/viatext \
    -Ithird_party/CLI \
    -Ithird_party/nlohmann \
    -Wall \
    -Wextra

# Core source and object files
CORE_SRCS = \
    src/core.cpp \
    src/message.cpp \
    src/message_id.cpp \
    src/arg_parser.cpp \
    src/routing.cpp

CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# CLI sources
CLI_MAIN = cli/main.cpp
CLI_OBJ = cli/main.o

# Target names
LIB_CORE = libviatext-core.a
CLI_BIN = cli/viatext-cli

# Default target: builds everything
all: $(LIB_CORE) $(CLI_BIN)

# Build static library from core object files
$(LIB_CORE): $(CORE_OBJS)
	ar rcs $@ $^

# Build CLI executable, linking with static library
$(CLI_BIN): $(CLI_OBJ) $(LIB_CORE)
	$(CXX) $(CXXFLAGS) -o $@ $(CLI_OBJ) $(LIB_CORE)

# Compile core source files to object files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile CLI main
cli/main.o: cli/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o cli/*.o $(LIB_CORE) $(CLI_BIN)

.PHONY: all clean
