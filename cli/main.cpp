/**
 * @file main.cpp
 * @brief Command‐line interface for ViaText Core (Linux‐only test harness).
 * @details
 *   This CLI tool allows you to interact with the ViaText core engine on a Linux host.
 *   It supports sending messages and directives, querying node status, and persisting
 *   minimal node state between runs. All underlying logic is delegated to viatext::Core.
 *
 *   Features:
 *     - Command‐line parsing via CLI11.
 *     - State persistence in ~/.config/altgrid/viatext-cli/<id>-node-state.json.
 *     - JSON wire format for inter‐core communication.
 *     - Optional “directive” messages vs. standard ViaText messages.
 *
 *   Usage examples:
 *     - Send a mesh message:
 *         viatext-cli --id leo -m "hello mesh"
 *     - Send a directive:
 *         viatext-cli --id leo -d -m "reboot"
 *     - Query status:
 *         viatext-cli --id leo --status
 *     - Persist default ID:
 *         viatext-cli --id leo --set-id
 *
 *   @author Leo
 *   @author ChatGPT
 *   @date   2025-08-04
 */

#include "CLI11.hpp"                ///< CLI11 argument parser
#include "viatext/core.hpp"         ///< ViaText core engine API
#include <iostream>                 ///< std::cout, std::cerr
#include <fstream>                  ///< std::ifstream, std::ofstream
#include <sstream>                  ///< std::stringstream
#include <chrono>                   ///< std::chrono for timestamps
#include <cstdlib>                  ///< std::getenv
#include <filesystem>               ///< std::filesystem for directory ops
#include <json.hpp>                 ///< nlohmann::json

namespace fs = std::filesystem;

/**
 * @brief Retrieve the user's home directory from the HOME environment variable.
 * @return std::string containing the path, or empty if HOME is not set.
 */
std::string get_home() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : "";
}

/**
 * @brief Compute the directory where CLI state files are stored.
 * @return Path to ~/.config/altgrid/viatext-cli/ (no trailing slash).
 */
std::string get_state_dir() {
    return get_home() + "/.config/altgrid/viatext-cli/";
}

/**
 * @brief Ensure that the CLI state directory exists, creating it if necessary.
 * @details
 *   Uses std::filesystem::create_directories(), which does nothing if the directory
 *   already exists, or creates parent directories as needed.
 */
void ensure_state_dir() {
    auto dir = get_state_dir();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

/**
 * @brief Construct the full path to the state file for a given node ID.
 * @param id  The node identifier.
 * @return Path to the state JSON file (<id>-node-state.json).
 */
std::string get_state_file(const std::string& id) {
    ensure_state_dir();
    return get_state_dir() + id + "-node-state.json";
}

/**
 * @brief Save minimal state (last-used ID and last tick time) to disk.
 * @param id         The node identifier.
 * @param last_time  The timestamp (ms) to persist as last_time.
 * @details
 *   Writes a JSON object with fields "id" and "last_time", formatted
 *   with 4-space indentation for readability.
 */
void save_state(const std::string& id, uint64_t last_time) {
    nlohmann::json j;
    j["id"]        = id;
    j["last_time"] = last_time;
    std::ofstream out(get_state_file(id));
    out << j.dump(4);
}

/**
 * @brief Load the last tick time from the state file for the given ID.
 * @param id  The node identifier.
 * @return The persisted last_time value, or 0 if file is missing or invalid.
 */
uint64_t load_state(const std::string& id) {
    std::ifstream in(get_state_file(id));
    if (!in) return 0;
    nlohmann::json j;
    in >> j;
    return j.value("last_time", 0);
}

/**
 * @brief Main entrypoint for the ViaText CLI.
 * @param argc  Argument count.
 * @param argv  Argument values.
 * @return Exit code (0 on success, non-zero on error).
 *
 * @details
 *   Parses command-line options, handles persistence flags, constructs a
 *   JSON message (either directive or viatext), feeds it into the core,
 *   advances the core by one tick, persists updated state, and prints
 *   any resulting events/messages to stdout.
 */
int main(int argc, char** argv) {
    // --- CLI11 Application Setup ---
    CLI::App app{"ViaText CLI (Linux-only) - test harness for core agent"};

    // Variables bound to CLI options
    std::string id, from, to, stamp, payload;
    int ttl = 0;
    bool status = false;
    bool as_directive = false;
    bool set_id = false;

    // Compute current time in ms since epoch
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
    // Define CLI options and flags
    app.add_option("--id", id, "Node/user id (REQUIRED)")
       ->required();
    app.add_option("--from", from, "Sender id (default: --id value)");
    app.add_option("--to", to, "Destination id (default: '0000')");
    app.add_option("--stamp", stamp, "Message stamp (default: epoch ms)");
    app.add_option("--payload,-m", payload, "Payload/message text");
    app.add_option("--ttl", ttl, "TTL/hops (default: 0)");
    app.add_flag("-d,--directive", as_directive, "Send as directive");
    app.add_flag("--status", status, "Print node status and exit");
    app.add_flag("--set-id", set_id, "Persist id to state file");

    // Parse the arguments
    CLI11_PARSE(app, argc, argv);

    // Apply defaults where flags/options were omitted
    if (from.empty())  from = id;
    if (to.empty())    to   = "0000";
    if (stamp.empty()) stamp = std::to_string(now);

    // Handle --set-id: persist ID and exit
    if (set_id) {
        save_state(id, now);
        std::cout << "ID '" << id << "' persisted in state file.\n";
        return 0;
    }

    // Instantiate core engine
    viatext::Core core;

    // Handle --status: print diagnostics and exit
    if (status) {
        auto stat = core.status();
        std::cout << "Node ID:    " << id << "\n"
                  << "Uptime:     " << stat.uptime_ms << " ms\n"
                  << "Inbox:      " << stat.inbox_size << " messages\n"
                  << "Outbox:     " << stat.outbox_size << " messages\n"
                  << "Errors:     " << stat.errors << "\n"
                  << "Last error: " << stat.last_error << "\n";
        return 0;
    }

    // --- Build the JSON message to feed into core ---
    std::string msg_json;
    if (as_directive) {
        // Directive message format
        nlohmann::json j;
        j["type"]    = "directive";
        j["from"]    = from;
        j["to"]      = to;
        j["stamp"]   = stamp;
        j["command"] = payload.empty() ? "noop" : payload;
        msg_json = j.dump();
    } else {
        // Standard ViaText message format
        nlohmann::json j;
        j["type"]    = "viatext";
        j["from"]    = from;
        j["to"]      = to;
        j["stamp"]   = stamp;
        j["ttl"]     = ttl;
        j["payload"] = payload.empty() ? "test message" : payload;
        msg_json = j.dump();
    }

    // Add message to core; exit on failure
    if (!core.add_message(msg_json)) {
        std::cerr << "Failed to add message (overflow or invalid).\n";
        return 2;
    }

    // Load previously saved tick time (if any)
    uint64_t last_time = load_state(id);
    uint64_t tick_time = now;
    if (last_time != 0 && last_time < now) {
        tick_time = now;
    }

    // Advance core by one tick
    core.tick(tick_time);

    // Persist updated tick time
    save_state(id, tick_time);

    // Drain and print all resulting outbox messages/events
    while (auto out = core.fetch_message()) {
        std::cout << *out << "\n";
    }

    return 0;
}
