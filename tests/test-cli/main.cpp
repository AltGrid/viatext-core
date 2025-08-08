// tests/test-cli/main.cpp
#include <iostream>
#include <string>
#include "viatext/text_fragments.hpp"
#include "viatext/arg_parser.hpp"

int main(int argc, char** argv) {
    // 1) Build a single raw string out of argv[]
    std::string raw;
    for (int i = 1; i < argc; ++i) {
        raw += argv[i];
        if (i < argc - 1) raw += ' ';
    }

    // 2) Parse it
    viatext::TextFragments<> frags(raw.c_str());
    viatext::ArgParser   args(frags);

    // 3) Print the directive (first token)
    std::cout << "Directive: " << args.directive() << "\n\n";

    // 4) Iterate and print standalone flags
    std::cout << "Flags:\n";
    if (args.flags().empty()) {
        std::cout << "  (none)\n";
    } else {
        for (const auto& flag : args.flags()) {
            std::cout << "  " << flag << "\n";
        }
    }
    std::cout << "\n";

    // 5) Iterate and print key→value arguments
    std::cout << "Arguments:\n";
    if (args.arguments().empty()) {
        std::cout << "  (none)\n";
    } else {
        for (auto it = args.arguments().begin(); it != args.arguments().end(); ++it) {
            std::cout << "  " << it->first << " = " << it->second << "\n";
        }
    }

    return 0;
}
