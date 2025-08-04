# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 \
    -Iinclude \
    -Ithird_party/CLI \
    -Ithird_party/nlohmann \
    -Ithird_party/ArduinoJson \
    -Ithird_party/ArduinoJson \
    -Wall \
    -Wextra

# Source/object files for core (add files here as you expand)
CORE_SRCS = src/core.cpp src/message.cpp src/parser.cpp src/routing.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# CLI main file
CLI_MAIN = cli/main.cpp
CLI_OBJ = cli/main.o

# Target names
LIB_CORE = libviatext-core.a
CLI_BIN = cli/viatext-cli

# Default target (builds both library and CLI)
all: $(LIB_CORE) $(CLI_BIN)

# Build static library from core object files
$(LIB_CORE): $(CORE_OBJS)
	ar rcs $@ $^

# Build CLI binary (depends on main.o and the library)
$(CLI_BIN): $(CLI_OBJ) $(LIB_CORE)
	$(CXX) $(CXXFLAGS) -o $@ $(CLI_OBJ) $(LIB_CORE)

# Compile core source files to object files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile CLI main
cli/main.o: cli/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o cli/*.o $(LIB_CORE) $(CLI_BIN) tests/test_*

.PHONY: all clean
