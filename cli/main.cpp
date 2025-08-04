#include "CLI11.hpp"
#include "viatext/core.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <json.hpp>

namespace fs = std::filesystem;

std::string get_home() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : "";
}

std::string get_state_dir() {
    return get_home() + "/.config/altgrid/viatext-cli/";
}

// Ensure the config directory exists
void ensure_state_dir() {
    auto dir = get_state_dir();
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

// Get the state file path for a given id
std::string get_state_file(const std::string& id) {
    ensure_state_dir();
    return get_state_dir() + id + "-node-state.json";
}

// Save state to file (minimal demo: just saves last-used id and timestamp)
void save_state(const std::string& id, uint64_t last_time) {
    nlohmann::json j;
    j["id"] = id;
    j["last_time"] = last_time;
    std::ofstream out(get_state_file(id));
    out << j.dump(4);
}

// Load state from file (returns last_time or 0)
uint64_t load_state(const std::string& id) {
    std::ifstream in(get_state_file(id));
    if (!in) return 0;
    nlohmann::json j;
    in >> j;
    return j.value("last_time", 0);
}

int main(int argc, char** argv) {
    CLI::App app{"ViaText CLI (Linux-only) - test harness for core agent"};

    // ---- Command-line arguments and flags ----
    std::string id, from, to, stamp, payload, directive;
    int ttl = 0;
    bool status = false;
    bool as_directive = false;
    bool set_id = false;

    // Default values
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // CLI options
    app.add_option("--id", id, "Node/user id (REQUIRED)")
        ->required();
    app.add_option("--from", from, "Sender id (default: --id value)");
    app.add_option("--to", to, "Destination id (default: 'BB2233')");
    app.add_option("--stamp", stamp, "Message stamp (default: epoch ms)");
    app.add_option("--payload,-m", payload, "Payload/message text");
    app.add_option("--ttl", ttl, "TTL/hops (default: 0)");
    app.add_flag("-d,--directive", as_directive, "Send as directive");
    app.add_flag("--status", status, "Print node status and exit");
    app.add_flag("--set-id", set_id, "Persist id to state file");

    CLI11_PARSE(app, argc, argv);

    // Fill in defaults
    if (from.empty()) from = id;
    if (to.empty()) to = "BB2233";
    if (stamp.empty()) stamp = std::to_string(now);

    // -- Handle ID setting (state file logic) --
    if (set_id) {
        save_state(id, now);
        std::cout << "ID '" << id << "' persisted in state file.\n";
        return 0;
    }

    // -- Print status if requested --
    viatext::Core core;
    if (status) {
        auto stat = core.status();
        std::cout << "Node ID:    " << id << "\n";
        std::cout << "Uptime:     " << stat.uptime_ms << " ms\n";
        std::cout << "Inbox:      " << stat.inbox_size << " messages\n";
        std::cout << "Outbox:     " << stat.outbox_size << " messages\n";
        std::cout << "Errors:     " << stat.errors << "\n";
        std::cout << "Last error: " << stat.last_error << "\n";
        return 0;
    }

    // ---- Build message ----
    std::string msg_json;
    if (as_directive) {
        nlohmann::json j;
        j["type"] = "directive";
        j["from"] = from;
        j["to"] = to;
        j["stamp"] = stamp;
        j["command"] = payload.empty() ? "noop" : payload;
        msg_json = j.dump();
    } else {
        nlohmann::json j;
        j["type"] = "viatext";
        j["from"] = from;
        j["to"] = to;
        j["stamp"] = stamp;
        j["ttl"] = ttl;
        j["payload"] = payload.empty() ? "test message" : payload;
        msg_json = j.dump();
    }

    // ---- Add message to core, tick, and fetch output ----
    if (!core.add_message(msg_json)) {
        std::cerr << "Failed to add message (overflow or invalid).\n";
        return 2;
    }

    // Try to load last tick time for this ID
    uint64_t last_time = load_state(id);
    uint64_t tick_time = now;
    if (last_time != 0 && last_time < static_cast<uint64_t>(now))
        tick_time = static_cast<uint64_t>(now);
    // else: use current time

    core.tick(tick_time);

    // Optionally, update state with new last_time
    save_state(id, tick_time);

    // Fetch and print output(s)
    while (auto out = core.fetch_message()) {
        std::cout << *out << std::endl;
    }

    return 0;
}
