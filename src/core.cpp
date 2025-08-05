/**
 * @file core.cpp
 * @brief Implementation of the Core class methods.
 * @details
 *   This file defines how the Core engine:
 *     1. Initializes its internal state (constructor).
 *     2. Advances time and triggers processing (`tick`).
 *     3. Accepts and validates incoming messages (`add_message`).
 *     4. Provides outgoing events/messages to the caller (`fetch_message`).
 *     5. Reports its current status (`status`).
 *     6. Processes exactly one queued message per tick (`process`).
 *     7. Parses and handles each message, emitting appropriate events (`handle_message`).
 *     8. Serializes events into JSON and queues them (`post_event`).
 *     9. Performs a lightweight validation on raw messages (`validate_message`).
 *
 *   See core.hpp for full API documentation and design rationale.
 */
#include "viatext/core.hpp"
#include "viatext/parser.hpp"

namespace viatext {

//
// 1. Constructor
//    Initializes all timers, counters, and error state to zero/empty.
//
Core::Core()
  : last_tick_(0),
    uptime_(0),
    errors_(0),
    last_error_("")
{}

//
// 2. tick(now)
//    Called by the wrapper to advance the core’s notion of time.
//    - If this is the first call, initializes last_tick_.
//    - Increments uptime_ by the elapsed ms (now − last_tick_).
//    - Updates last_tick_ to the new time.
//    - Calls process() to handle one message.
//
void Core::tick(timepoint_t now) {
    if (last_tick_ == 0) {
        // First-ever tick: seed the baseline time
        last_tick_ = now;
    }
    // Add elapsed time to uptime_ (ignore negative jumps)
    uptime_ += (now > last_tick_) ? (now - last_tick_) : 0;
    last_tick_ = now;

    // Perform core logic for this tick
    process();
}

//
// 3. add_message(msg_json)
//    Enqueue an incoming message for later processing.
//    - validate_message() checks size and non-emptiness.
//    - If validation fails or inbox is full, increments errors_, logs last_error_, emits an error event, and returns false.
//    - Otherwise pushes the raw JSON into inbox_ and returns true.
//
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

bool Core::add_message(const std::string& msg_raw, const bool raw) {
    if (raw == true){
        
    } else {

    }

}

//
// 4. fetch_message()
//    Dequeue the next outgoing JSON event/message.
//    - If outbox_ is empty, returns std::nullopt.
//    - Otherwise pops the front string and returns it.
//
std::optional<std::string> Core::fetch_message() {
    if (outbox_.empty()) {
        return std::nullopt;
    }
    std::string msg = outbox_.front();
    outbox_.pop();
    return msg;
}

//
// 5. status()
//    Return a snapshot of internal metrics:
//      - uptime_ms, inbox_size, outbox_size, errors count, last_error message.
//
Status Core::status() const {
    return Status{
        uptime_,
        inbox_.size(),
        outbox_.size(),
        errors_,
        last_error_
    };
}

//
// 6. process()
//    Core processing loop: called once per tick().
//    - If inbox_ is empty, do nothing.
//    - Otherwise pop one message and pass it to handle_message().
//
void Core::process() {
    if (inbox_.empty()) {
        return;
    }
    std::string msg_json = inbox_.front();
    inbox_.pop();
    handle_message(msg_json);
}

//
// 7. handle_message(msg_json)
//    Parse and dispatch a single message.
//    - Uses parser::from_json to convert JSON→Message; on failure, logs and emits an error.
//    - Retrieves the “type” field via parser::get_type.
//    - If type=="directive", emits an “ack” for directive.
//    - If type=="viatext", emits an “ack” for a mesh message.
//    - Otherwise emits an error for unknown type.
//
bool Core::handle_message(const std::string& msg_json) {
    // Parse JSON into Message object
    auto obj = parser::from_json(msg_json);
    if (!obj) {
        last_error_ = "Failed to parse message JSON";
        post_event("error", last_error_);
        ++errors_;
        return false;
    }

    // Determine message category
    std::string type = parser::get_type(msg_json);

    if (type == "directive") {
        // Acknowledge handling of a directive
        post_event("ack", "Directive processed: " + msg_json);
        return true;
    }
    else if (type == "viatext") {
        // Acknowledge a regular mesh message
        post_event("ack", "ViaText message processed: " + msg_json);
        return true;
    }
    else {
        // Unknown or unsupported type
        last_error_ = "Unknown message type: " + type;
        post_event("error", last_error_);
        ++errors_;
        return false;
    }
}

//
// 8. post_event(type, detail)
//    Helper to serialize {type, detail} into JSON via parser::event_json,
//    and enqueue it onto outbox_.
//
void Core::post_event(const std::string& type,
                      const std::string& detail) {
    outbox_.push(parser::event_json(type, detail));
}

//
// 9. validate_message(msg)
//    Quick sanity check on a raw message string:
//      - Must not be empty.
//      - Must not exceed MAX_MSG_LEN.
//    Returns true if message is “well-formed enough” to enqueue.
//
bool Core::validate_message(const std::string& msg) const {
    return !msg.empty() && msg.size() <= MAX_MSG_LEN;
}

} // namespace viatext
