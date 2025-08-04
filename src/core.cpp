#include "viatext/core.hpp"
#include "viatext/parser.hpp"

namespace viatext {

Core::Core() : last_tick_(0), uptime_(0), errors_(0), last_error_("") {}

void Core::tick(timepoint_t now) {
    if (last_tick_ == 0) last_tick_ = now;
    uptime_ += (now > last_tick_) ? (now - last_tick_) : 0;
    last_tick_ = now;
    process();
}

bool Core::add_message(const std::string& msg_json) {
    if (!validate_message(msg_json)) {
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
    inbox_.push(msg_json);
    return true;
}

std::optional<std::string> Core::fetch_message() {
    if (outbox_.empty())
        return std::nullopt;
    std::string msg = outbox_.front();
    outbox_.pop();
    return msg;
}

Status Core::status() const {
    return Status{
        uptime_,
        inbox_.size(),
        outbox_.size(),
        errors_,
        last_error_
    };
}

void Core::process() {
    if (inbox_.empty())
        return;
    std::string msg_json = inbox_.front();
    inbox_.pop();
    handle_message(msg_json);
}

bool Core::handle_message(const std::string& msg_json) {
    // Always parse using parser (handles all validation)
    auto obj = parser::from_json(msg_json);
    if (!obj) {
        last_error_ = "Failed to parse message JSON";
        post_event("error", last_error_);
        ++errors_;
        return false;
    }

    // Determine message type from JSON
    std::string type = parser::get_type(msg_json);


    if (type == "directive") {
        // Handle directives (e.g., reboot, set config)
        post_event("ack", "Directive processed: " + msg_json);
        return true;
    } else if (type == "viatext") {
        // Mesh/regular message
        post_event("ack", "ViaText message processed: " + msg_json);
        return true;
    } else {
        last_error_ = "Unknown message type: " + type;
        post_event("error", last_error_);
        ++errors_;
        return false;
    }
}

void Core::post_event(const std::string& type, const std::string& detail) {
    outbox_.push(parser::event_json(type, detail));
}

bool Core::validate_message(const std::string& msg) const {
    return msg.size() <= MAX_MSG_LEN && !msg.empty();
}

} // namespace viatext
