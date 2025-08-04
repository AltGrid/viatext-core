#pragma once

#include <string>
#include <queue>
#include <optional>
#include <cstdint>
#include <vector>
#include <chrono>
#include "viatext/message.hpp"

namespace viatext {

// Message struct for status, error, events
struct Status {
    uint64_t uptime_ms;
    size_t inbox_size;
    size_t outbox_size;
    size_t errors;
    std::string last_error;
};

class Core {
public:
    // === Types ===
    using timepoint_t = uint64_t; // ms since boot or epoch

    // === Configuration ===
    static constexpr size_t MAX_QUEUE = 16;      // Inbox/outbox queue cap
    static constexpr size_t MAX_MSG_LEN = 256;   // Max message size (chars)

    // === Constructor/Destructor ===
    Core();

    // === API ===

    // Called by wrapper to advance time and process actions.
    // Pass time in ms (since boot or epoch—just be consistent across nodes).
    void tick(timepoint_t now);

    // Add an incoming message (from user, mesh, or directive).
    // Returns true if queued, false if rejected (overflow or bad format).
    bool add_message(const std::string& msg);

    // Fetch next output/event/error message, if any (else empty).
    // Wrapper should call this after each tick.
    std::optional<std::string> fetch_message();

    // Get internal status (uptime, queue sizes, errors)
    Status status() const;

private:
    // === State ===
    timepoint_t last_tick_ = 0;
    timepoint_t uptime_ = 0;      // ms since init
    size_t errors_ = 0;
    std::string last_error_;

    // Message queues
    std::queue<std::string> inbox_;     // Incoming: to process
    std::queue<std::string> outbox_;    // Outgoing: to wrapper
    // You could switch to std::deque or std::vector if needed

    // === Internal Methods ===
    void process();                     // Process one or more messages
    bool handle_message(const std::string& msg); // Main handler (returns success)

    // Helper: Post error/event to outbox
    void post_event(const std::string& type, const std::string& detail);

    // Validate message size/type etc.
    bool validate_message(const std::string& msg) const;
};

} // namespace viatext
