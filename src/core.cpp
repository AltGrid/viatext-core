#include "core.hpp"

namespace viatext {

// Constructor: When a ViaTextCore object is created, set all counters to zero and set the node's ID if provided.
// The STL containers (queues, sets) are empty by default—no need to explicitly clear them.
ViaTextCore::ViaTextCore(const std::string& optional_node_id)
    : node_id(optional_node_id), tick_count(0), uptime(0), last_timestamp(0)
{
    // All containers auto-init empty
}

// Add a new message string to the inbox queue. This could come from the CLI, radio, or any source.
// The actual processing will happen later (on a tick).
void ViaTextCore::add_message(const std::string& arg_string) {
    inbox.push(arg_string);
}

// On each tick, we update time counters and process one message from the inbox.
// This keeps the system moving forward at a steady pace and allows for time-based logic later.
void ViaTextCore::tick(uint64_t current_timestamp) {
    if (last_timestamp == 0)
        last_timestamp = current_timestamp;
    uptime += (current_timestamp - last_timestamp);
    last_timestamp = current_timestamp;
    tick_count += 1;
    process();
}

// Main process function: takes one message from the inbox and handles it.
// ArgParser breaks the string into flags and arguments.
// We use if/else-if to select the right handler for each supported command/flag.
void ViaTextCore::process() {
    if (inbox.empty()) return;
    std::string arg_string = inbox.front();
    inbox.pop();

    ArgParser parser(arg_string);
    std::string msg_type = parser.get_message_type();

    // Each flag/command (e.g., "-m", "-p") is handled by its own function.
    // This pattern is easy to follow for beginners, and keeps logic modular.
    if      (msg_type == "-m")        handle_message(parser);         // Handle a standard message
    else if (msg_type == "-p")        handle_ping(parser);            // Handle a ping request
    else if (msg_type == "-mr")       handle_mesh_report(parser);     // Handle mesh status request
    else if (msg_type == "-ack")      handle_acknowledge(parser);     // Handle acknowledgments
    else if (msg_type == "-save")     handle_save(parser);            // Save command (stubbed)
    else if (msg_type == "-load")     handle_load(parser);            // Load command (stubbed)
    else if (msg_type == "-get_time") handle_get_time(parser);        // Get time command (stubbed)
    else if (msg_type == "-set_time") handle_set_time(parser);        // Set time command (stubbed)
    else if (msg_type == "-id")       set_node_id(parser.get_arg("-id")); // Set this node's ID
    // Unrecognized flags are simply dropped (could log for debugging).
}

// This handles a standard message (flag "-m").
// We first check if we've already seen this message by its sequence ID (deduplication).
// If it's addressed to us, we prepare an ACK if needed, and queue the message for delivery or logging.
void ViaTextCore::handle_message(const ArgParser& parser) {
    Message message(parser.get_arg("-data"));

    // If we've seen this sequence before, ignore (prevents replay/double-processing).
    if (received_message_ids.count(message.get_id().sequence))
        return; // Already processed
    received_message_ids.insert(message.get_id().sequence);

    // If this message is addressed to us, handle it.
    if (message.get_to() == node_id) {
        // If the sender wants an acknowledgment, build and queue an ACK message.
        if (message.requests_acknowledgment()) {
            std::string ack_args = build_ack_args(message);
            outbox.push(ack_args);
        }
        // Always queue a delivery/logging message for the wrapper/app to handle.
        std::string delivered_args = build_received_args(message);
        outbox.push(delivered_args);
    }
    // If it's not for us, we do nothing here; wrapper can handle forwarding if desired.
}

// Handles ping requests (flag "-p"). Always replies with "-pong" and this node's ID.
void ViaTextCore::handle_ping(const ArgParser&) {
    std::string reply_args = "-pong -from " + node_id;
    outbox.push(reply_args);
}

// Handles mesh report/status requests (flag "-mr").
// For now, just returns 0 neighbors (stub); real implementation would count known peers.
void ViaTextCore::handle_mesh_report(const ArgParser&) {
    std::string mesh_args = "-mr -from " + node_id + " -neigh_count " + std::to_string(get_neighbor_count());
    outbox.push(mesh_args);
}

// Handles acknowledgment messages (flag "-ack").
// The actual effect (removing a delivered message, logging the event) is left for the app/wrapper.
void ViaTextCore::handle_acknowledge(const ArgParser& parser) {
    std::string ack_id = parser.get_arg("-msg_id");
    // Application/wrapper handles display, removal, etc.
}

// Set this node's callsign/ID.
// This can be set at startup or at runtime when a "-id" command arrives.
void ViaTextCore::set_node_id(const std::string& new_id) {
    node_id = new_id;
}

// Returns and removes the next outbound message, if there is one. Otherwise returns an empty string.
// The wrapper or application should call this to get messages to send.
std::string ViaTextCore::get_message() {
    if (outbox.empty()) return "";
    std::string msg = outbox.front();
    outbox.pop();
    return msg;
}

// Utility: Given a message, builds the correct argument string to acknowledge it.
// This uses the current node's ID as the sender, and copies the sequence ID.
std::string ViaTextCore::build_ack_args(const Message& message) const {
    return "-ack -from " + node_id +
           " -to " + message.get_from() +
           " -msg_id " + std::to_string(message.get_id().sequence);
}

// Utility: Builds a standard argument string representing a received message for logging/delivery.
// Includes the original sender and the message serialized in wire format.
std::string ViaTextCore::build_received_args(const Message& message) const {
    return "-r -from " + message.get_from() +
           " -data " + message.to_wire_string();
}

} // namespace viatext
