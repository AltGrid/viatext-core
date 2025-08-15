# ViaText CLI (Linux) — Makefile
#
# Rationale:
# - Tiny, portable, auto-discovers all src/*.cpp
# - Debug/Release toggles, optional sanitizers, optional "portable" linking
# - Optional FS_LIB for older libstdc++ that requires -lstdc++fs
#
# Usage:
#   make                 # build (default, -O2)
#   make debug           # -O0 -g
#   make release         # -O2 -s
#   make SAN=1           # address+UB sanitizers
#   make PORTABLE=1      # -static-libstdc++ -static-libgcc
#   make FS_LIB=-lstdc++fs   # only if your toolchain needs it for <filesystem>
#   make run DEV=/dev/ttyACM0 ARGS="--get-id"
#   make run-get-id
#   make run-ping
#   make run-set-id ID=vt-01
#   make run-scan        # does --scan (and accepts ALIASES=1 to add links)
#   make run-node ID=vt-01 ARGS="--get-id"
#   make clean
#
# Note: We link against your host libstdc++ (default). Rebuild on each host.

# --- toolchain ---
CXX ?= g++
AR  ?= ar

# --- project layout ---
APP      := viatext-cli
SRC_DIR  := src
INC_DIRS := include third_party/CLI
OBJ_DIR  := build

# --- sources / objects ---
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# --- base flags ---
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -MMD -MP
CXXFLAGS += $(addprefix -I,$(INC_DIRS))

# Sanitizers (dev only)
SAN ?=
ifeq ($(SAN),1)
  CXXFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS  += -fsanitize=address,undefined
endif

# Optional portable link
PORTABLE ?=
ifeq ($(PORTABLE),1)
  LDFLAGS += -static-libstdc++ -static-libgcc
endif

# Optional filesystem lib (older GCC libstdc++ needs this for <filesystem>)
FS_LIB ?=
LDFLAGS += $(FS_LIB)

# Default optimization
CXXFLAGS += -O2

# --- targets ---
.PHONY: all debug release clean run dirs run-get-id run-ping run-set-id run-scan run-node

all: $(APP)

debug: CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O0 -g
debug: clean all

release: CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O2 -s
release: clean all

dirs:
	@mkdir -p $(OBJ_DIR)

$(APP): dirs $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Compile .cpp -> .o with depfile generation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- run helpers ---
DEV  ?= /dev/ttyACM0
ARGS ?=
run: $(APP)
	./$(APP) --dev $(DEV) $(ARGS)

# Quick tests
run-get-id: $(APP)
	./$(APP) --dev $(DEV) --get-id

run-ping: $(APP)
	./$(APP) --dev $(DEV) --ping

# Usage: make run-set-id ID=vt-01 DEV=/dev/ttyACM0
ID ?= vt-01
run-set-id: $(APP)
	./$(APP) --dev $(DEV) --set-id $(ID)

# Scan helpers
# Usage: make run-scan                # just list nodes
#        make run-scan ALIASES=1      # also create aliases in $XDG_RUNTIME_DIR/viatext
ALIASES ?= 0
run-scan: $(APP)
	./$(APP) --scan $(if $(filter 1,$(ALIASES)),--aliases,)

# Target by node ID via alias/scan resolve:
# Usage: make run-node ID=vt-01 ARGS="--get-id"
run-node: $(APP)
	./$(APP) --node $(ID) $(ARGS)

clean:
	@rm -rf $(OBJ_DIR) $(APP)

# Auto-include generated dep files
-include $(DEPS)
