#include "viatext/core.hpp"
#include <sstream>
#include <json.hpp> // Or use your own minimal JSON, or plain string if you wish

namespace viatext {

// -- Constructor
Core::Core() : last_tick_(0), uptime_(0), errors_(0), last_error_("") {}

// -- Advance time and process queue
void Core::tick(timepoint_t now) {
    if (last_tick_ == 0) last_tick_ = now; // Init on first tick
    uptime_ += (now > last_tick_) ? (now - last_tick_) : 0;
    last_tick_ = now;
    process();
}

// -- Add message to inbox
bool Core::add_message(const std::string& msg) {
    if (!validate_message(msg)) {
        ++errors_;
        last_error_ = "Message too long or malformed";
        post_event("error", last_error_);
        return false;
    }
    if (inbox_.size() >= MAX_QUEUE) {
        ++errors_;
        last_error_ = "Inbox queue overflow";
        post_event("error", last_error_);
        return false;
    }
    inbox_.push(msg);
    return true;
}

// -- Fetch next output message/event/error
std::optional<std::string> Core::fetch_message() {
    if (outbox_.empty())
        return std::nullopt;
    std::string msg = outbox_.front();
    outbox_.pop();
    return msg;
}

// -- Get current status struct
Status Core::status() const {
    return Status{
        uptime_,
        inbox_.size(),
        outbox_.size(),
        errors_,
        last_error_
    };
}

// -- Internal: Process messages in inbox
void Core::process() {
    // For now: process one message per tick (could be all, or N per tick)
    if (inbox_.empty())
        return;
    std::string msg = inbox_.front();
    inbox_.pop();

    if (!handle_message(msg)) {
        // Already posted error inside handle_message
    }
    // Could: handle time-based events, retries, periodic tasks here as well
}

// -- Internal: Message handler (dummy for now)
bool Core::handle_message(const std::string& msg) {
    // Try to parse JSON to get type, content (if desired)
    try {
        auto obj = nlohmann::json::parse(msg);
        std::string type = obj.value("type", "unknown");
        if (type == "directive") {
            // Handle command (reboot, config, etc.)
            post_event("ack", "Directive processed: " + msg);
            return true;
        } else if (type == "viatext") {
            // Real mesh message; in real system, handle as needed
            post_event("ack", "ViaText message processed: " + msg);
            return true;
        } else {
            last_error_ = "Unknown message type: " + type;
            post_event("error", last_error_);
            ++errors_;
            return false;
        }
    } catch (...) {
        last_error_ = "Failed to parse JSON message";
        post_event("error", last_error_);
        ++errors_;
        return false;
    }
}

// -- Internal: Post error/event/ack to outbox as JSON
void Core::post_event(const std::string& type, const std::string& detail) {
    nlohmann::json obj;
    obj["type"] = type;
    obj["detail"] = detail;
    outbox_.push(obj.dump());
}

// -- Internal: Message validation
bool Core::validate_message(const std::string& msg) const {
    return msg.size() <= MAX_MSG_LEN && !msg.empty();
}

} // namespace viatext
