// cli/main.cpp

#include <CLI11.hpp>
#include <iostream>
#include <string>
#include "viatext/core.hpp"

int main(int argc, char** argv) {
    CLI::App app{"ViaText CLI Test Harness"};

    std::string node_id = "CLI1";
    bool print_output = true;
    std::string cmd_string;
    std::string r;
 

    // CLI flags and options
    app.add_option("-n,--node", node_id, "Node ID/callsign for this session");
    app.add_option("command", cmd_string, "ViaText protocol command string (e.g. '-m -data ...')");
    app.add_option("-r", cmd_string, "Raw input, straight to node. Example: command -r \"--set_id HCKRmn\"");
    app.add_flag("-q,--quiet", print_output, "Suppress output from core");

    CLI11_PARSE(app, argc, argv);

    viatext::ViaTextCore core(node_id);

    // Pass the user command (if any) into the core
    if (!cmd_string.empty()) {
        core.add_message(cmd_string);
    }

    // Run one tick for simplicity
    core.tick(static_cast<uint64_t>(time(nullptr)) * 1000);

    // Optionally print output (echo/print mode)
    if (print_output) {
        std::string out = core.get_message();
        std::cout << out << std::endl;
    }

    return 0;
}
