CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -Ithird_party/CLI -Wall -Wextra

# Object files for core
CORE_OBJS = src/core.o src/parser.o src/routing.o

# Build static library
libviatext-core.a: $(CORE_OBJS)
	ar rcs $@ $^

# Build CLI binary
cli/viatext-cli: cli/main.o libviatext-core.a
	$(CXX) $(CXXFLAGS) -o $@ cli/main.o libviatext-core.a

# Build objects
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
cli/main.o: cli/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f src/*.o cli/*.o libviatext-core.a cli/viatext-cli
