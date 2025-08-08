# Makefile for ViaText-Core

CXX      := g++
CXXFLAGS := -std=c++17 \
            -Iinclude \
            -Ithird_party \
            -Wall \
            -Wextra

# Core sources and objects
CORE_SRCS := \
    src/message.cpp \
    src/message_id.cpp \
    src/text_fragments.cpp

CORE_OBJS := $(CORE_SRCS:.cpp=.o)

LIB_CORE := libviatext-core.a

# Default target
all: $(LIB_CORE) test-cli test-message-id

# Build static library
$(LIB_CORE): $(CORE_OBJS)
	ar rcs $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ------------------------
# test-cli (CLI 1)
# ------------------------

TEST_CLI_SRC := tests/test-cli/main.cpp
TEST_CLI_OBJ := $(TEST_CLI_SRC:.cpp=.o)
TEST_CLI_BIN := tests/tcli

test-cli: $(TEST_CLI_OBJ) $(LIB_CORE)
	$(CXX) $(CXXFLAGS) -o $(TEST_CLI_BIN) $(TEST_CLI_OBJ) $(LIB_CORE)

tests/test-cli/%.o: tests/test-cli/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ------------------------
# test-message-id (CLI 2)
# ------------------------

MSGID_SRC := tests/test-message-id/main.cpp
MSGID_OBJ := $(MSGID_SRC:.cpp=.o)
MSGID_BIN := tests/tmid

test-message-id: $(MSGID_OBJ) $(LIB_CORE)
	$(CXX) $(CXXFLAGS) -o $(MSGID_BIN) $(MSGID_OBJ) $(LIB_CORE)

tests/test-message-id/%.o: tests/test-message-id/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ------------------------
# Clean
# ------------------------

clean:
	rm -f src/*.o tests/test-cli/*.o tests/test-message-id/*.o \
	      $(LIB_CORE) test-cli test-message-id

.PHONY: all clean
