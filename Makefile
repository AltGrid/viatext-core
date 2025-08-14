# ViaText CLI (Linux) — Makefile
#
# Usage:
#   make            # build (default)
#   make SAN=1      # build with ASan/UBSan
#   make debug      # debug build (-O0 -g)
#   make release    # release build (-O2 -s)
#   make run DEV=/dev/ttyACM0 ARGS="--get-id"
#   make clean
#
# Requirements:
#   - g++ with C++17
#   - No external libs needed (uses termios/poll from glibc)
#
# Tree:
#   include/slip.hpp
#   third_party/CLI/CLI11.hpp
#   src/main.cpp     (your CLI entry)

# --- toolchain ---
CXX      ?= g++
AR       ?= ar

# --- project ---
APP      := viatext-cli
SRC_DIR  := src
INC_DIRS := include third_party/CLI
OBJ_DIR  := build

# --- sources / objects ---
SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS     := $(OBJS:.o=.d)

# --- base flags ---
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
CXXFLAGS += $(addprefix -I,$(INC_DIRS))

# --- sanitizer toggle (make SAN=1) ---
SAN ?=
ifeq ($(SAN),1)
  CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS  += -fsanitize=address,undefined
endif

# --- default build type (can be overridden by 'make debug' / 'make release') ---
CXXFLAGS += -O2

# --- targets ---
.PHONY: all debug release clean run dirs

all: $(APP)

debug: CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O0 -g
debug: clean all

release: CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O2 -s
release: clean all

dirs:
	@mkdir -p $(OBJ_DIR)

$(APP): dirs $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile .cpp -> .o with depfile
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run helper: device + args
# Example: make run DEV=/dev/serial/by-id/usb-... ARGS="--get-id"
DEV  ?= /dev/ttyACM0
ARGS ?=
run: $(APP)
	./$(APP) --dev $(DEV) $(ARGS)

clean:
	@rm -rf $(OBJ_DIR) $(APP)

# Include auto-generated dep files
-include $(DEPS)
