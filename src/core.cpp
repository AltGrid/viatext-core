/**
 * @file core.cpp
 * @brief ViaText Core implementation — main protocol event loop and handler.
 *
 * See core.hpp for interface and documentation.
 * Only ETL containers and fixed-length strings, no dynamic memory, STL, or RTTI.
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-09
 */

#include "viatext/core.hpp"

namespace viatext {

// --- Constructor ---

// Initialize all state variables for the Core object.
// The node_id (call sign) can be left empty, or set via constructor argument.
// All counters and queues are zeroed out to a safe, known state.
Core::Core(const char* optional_node_id)
    : node_id(),           // Empty callsign by default (max 6 chars, set below)
      tick_count(0),       // How many ticks have happened since start (increments each tick)
      uptime(0),           // Milliseconds since this node/core was started
      last_timestamp(0),   // Timestamp of last tick (ms, set during first tick)
      inbox(),             // Inbound queue: holds incoming raw message strings (max INBOX_SIZE)
      outbox(),            // Outbound queue: holds outgoing message strings (max OUTBOX_SIZE)
      received_message_ids() // Rolling list of recently seen message IDs (for deduplication)
{
    // Set node_id: copy up to 6 chars from argument, or leave blank if null/empty
    set_node_id(optional_node_id ? optional_node_id : "");
}

// --- Add inbound message ---

// Add a new inbound message (as a raw argument string) to the inbox queue.
// If the inbox is already full, do not add the message and return false.
// Otherwise, wrap the string in an ETL string (string_t) and add it to the queue.
// This function is used by wrappers to submit messages for processing.
bool Core::add_message(const char* arg_string) {
    if (inbox.full())        // Check if the inbox queue is at max capacity
        return false;        // Inbox full: reject new message
    inbox.push_back(string_t(arg_string)); // Copy and queue the new message string
    return true;             // Success
}


// --- Tick: update time, process one message ---
// Called once per event loop iteration or hardware timer interrupt.
// Updates all internal time counters using the current timestamp (typically millis() on MCU).
// - If this is the very first tick, initialize last_timestamp to the given value.
// - Adds the elapsed time to the total uptime counter.
// - Updates the last_timestamp to the current tick's value for next round.
// - Increments the tick counter (how many ticks since boot/reset).
// - Processes a single message from the inbox (if present).
void Core::tick(uint32_t current_timestamp) {
    if (last_timestamp == 0)           // Handle the very first call (start timer)
        last_timestamp = current_timestamp;
    uptime += (current_timestamp - last_timestamp); // Accumulate elapsed time since last tick
    last_timestamp = current_timestamp;             // Save this tick's timestamp for next round
    tick_count++;                                  // Track number of ticks since init
    process();                                     // Process one message from the inbox (if available)
}


// --- Get outbound message for wrapper ---
// This function is called by the wrapper (Linux CLI, daemon, or MCU handler)
// to retrieve the next message that should be sent out from this node.
// - If outbox is empty, return false (no message to send).
// - Otherwise, copy up to max_len-1 bytes of the first message into out_buf (leaving space for null terminator).
// - After copying, remove the message from the outbox (FIFO).
// - Always null-terminate the buffer, even if truncated.
// - Returns true if a message was copied to out_buf.
bool Core::get_message(char* out_buf, size_t max_len) {
    if (outbox.empty()) return false; // No outgoing message available
    const string_t& s = outbox.front(); // Reference to next outbound message
    size_t to_copy = s.size() < max_len - 1 ? s.size() : max_len - 1; // Determine how much can fit
    memcpy(out_buf, s.c_str(), to_copy); // Copy message content
    out_buf[to_copy] = '\0'; // Null-terminate output
    outbox.erase(outbox.begin()); // Remove message from outbox queue
    return true; // Indicate message was returned
}


// --- Set node id (safe, truncated to 6 chars) ---
// Updates this node's unique identifier/callsign (node_id).
// - Clears any existing ID first.
// - Copies up to 6 characters from new_id (ignores extra characters).
// - Safe if new_id is nullptr (will set to empty).
// Typical usage: set from CLI, device config, or a received -id message.
void Core::set_node_id(const char* new_id) {
    node_id.clear();                  // Remove any previous value
    if (new_id) node_id.assign(new_id, 6); // Assign up to 6 chars, or empty if null
}


// --- MAIN PROCESS FUNCTION ---
// Pops one message from inbox and dispatches to the correct handler
// based on the directive/flag (e.g. -m, -p, -ack, etc).
// This is the central event switchboard of ViaText Core.
//
// To add a new command/feature, simply insert another "else if" block
// using the same pattern below.

void Core::process() {
    if (inbox.empty())
        return; // Nothing to process

    // Step 1: Get next raw arg string from inbox (FIFO)
    string_t arg_string = inbox.front();
    inbox.erase(inbox.begin()); // Remove after fetching

    // Step 2: Parse into ArgParser (splits into tokens/flags/kv-pairs)
    TextFragments<8, 32> fragments(arg_string.c_str());
    ArgParser parser(fragments);

    // First token (directive/command/flag)
    const etl::string<8>& msg_type = parser.directive();

    // --- Dispatch: one if/else chain per supported command ---
    // This is the main flow; keep each line clean for future additions.

    // argument: -m  (message directive; process a normal inbound message)
    // If the directive is "-m", dispatch to message handler
    if      (msg_type == "-m")
        handle_message(parser);

    //
    // argument: -p  (ping; used for liveness check or neighbor discovery)
    // If the directive is "-p", reply to ping (usually with -pong)
    else if (msg_type == "-p")
        handle_ping(parser);

    //
    // argument: -mr  (mesh report; request node's mesh status info)
    // If the directive is "-mr", respond with mesh report (neighbor count, etc)
    else if (msg_type == "-mr")
        handle_mesh_report(parser);

    //
    // argument: -ack  (acknowledgment; response to messages requesting ACK)
    // If the directive is "-ack", process ACK logic (dedup, update state, app event)
    else if (msg_type == "-ack")
        handle_acknowledge(parser);

    //
    // argument: -save  (save state or data; typically a stub for wrappers)
    // If the directive is "-save", run save handler (wrapper-defined, often a stub)
    else if (msg_type == "-save")
        handle_save(parser);

    //
    // argument: -load  (load state or data; typically a stub for wrappers)
    // If the directive is "-load", run load handler (wrapper-defined, often a stub)
    else if (msg_type == "-load")
        handle_load(parser);

    //
    // argument: -get_time  (query the current node/system time)
    // If the directive is "-get_time", reply with time handler (can be a stub)
    else if (msg_type == "-get_time")
        handle_get_time(parser);

    //
    // argument: -set_time  (set the node/system time, typically from a leader)
    // If the directive is "-set_time", run time set handler (can be a stub)
    else if (msg_type == "-set_time")
        handle_set_time(parser);


    // "Special" handler for dynamic ID assignment via -id
    else if (msg_type == "-id") {
        // Get argument for -id and assign as node ID
        etl::string<8> val = parser.get_argument("-id");
        set_node_id(val.c_str());
    }

    // else: unknown directive, silently ignore (could add logging here)
    // This makes it easy to add new handlers: just copy/paste an "else if"
}



// --- MESSAGE HANDLER ---

// Handles -m: deduplicate, ACK, deliver, queue reply if addressed here.
void Core::handle_message(const ArgParser& parser) {
    // Attempt to retrieve the -data argument from the parsed input (should be the raw wire-format message)
    const auto& wire_str = parser.get_argument("-data");
    if (wire_str.empty()) return; // If no -data, nothing to do

    // Convert the wire string into a Message object for structured access to fields (ID, from, to, etc)
    Message message(wire_str.c_str());

    // Check if this message's ID has already been seen (prevents duplicate processing)
    if (has_seen_message(message.id.sequence)) return;

    // Remember this message ID so we don't process duplicates in the near future
    remember_message_id(message.id.sequence);

    // If this message is addressed to us (our node ID), handle delivery and response
    if (message.to == node_id) {
        // If the message requests an acknowledgment, prepare and queue an ACK reply
        if (message.id.requests_acknowledgment()) {
            char ack_buf[64];
            build_ack_args(message, ack_buf, sizeof(ack_buf));
            if (!outbox.full()) outbox.push_back(string_t(ack_buf));
        }

        // Always queue a "received" reply (used for logs, UI, or sender tracking)
        char rec_buf[128];
        build_received_args(message, rec_buf, sizeof(rec_buf));
        if (!outbox.full()) outbox.push_back(string_t(rec_buf));
    }

    // Note: Forwarding or relaying of messages is managed by the wrapper, not by core logic.
}


// --- PING HANDLER ---

// Replies with -pong and this node's ID.
void Core::handle_ping(const ArgParser&) {
    // Prepare a reply message string in the format: "-pong -from <our node_id>"
    char reply[32];
    snprintf(reply, sizeof(reply), "-pong -from %s", node_id.c_str());

    // Add the reply to the outbound queue, unless it is already full
    if (!outbox.full()) outbox.push_back(string_t(reply));
}


// --- MESH REPORT HANDLER ---

// Replies with mesh report (stub; always zero neighbors for now).
void Core::handle_mesh_report(const ArgParser&) {
    // Create a report string showing:
    // - Directive: -mr
    // - Our node ID
    // - Neighbor count (currently always returns 0 from get_neighbor_count())
    char mesh_buf[48];
    snprintf(mesh_buf, sizeof(mesh_buf), "-mr -from %s -neigh_count %d",
             node_id.c_str(), get_neighbor_count());

    // Add the report to the outbound queue, unless the queue is full
    if (!outbox.full()) outbox.push_back(string_t(mesh_buf));
}

// --- ACK HANDLER ---

// Handles an incoming -ack directive (acknowledgment of a sent message).
void Core::handle_acknowledge(const ArgParser& parser) {
    // Allocate a string to store the ID of the message being acknowledged.
    etl::string<16> ack_id;

    // Extract the value of the -msg_id argument from the parsed message.
    // NOTE: The returned value is not currently assigned to ack_id,
    // so this call has no effect on ack_id yet.
    parser.get_argument("-msg_id");

    // Application/wrapper logic would go here.
    // For example:
    //  - Mark the original message as delivered.
    //  - Remove it from any pending retransmission queues.
    //  - Update UI to reflect successful delivery.
}


// --- SAVE/LOAD/GET/SET_TIME HANDLERS (stubs) ---

// Handles the -save directive.
// Intended to instruct the wrapper/application to persist current state
// (e.g., configuration, routing table, logs) to non-volatile storage.
// Not yet implemented — placeholder for future logic.
void Core::handle_save(const ArgParser&) {}

// Handles the -load directive.
// Intended to instruct the wrapper/application to load previously saved state
// from storage and restore it into the running system.
// Not yet implemented — placeholder for future logic.
void Core::handle_load(const ArgParser&) {}

// Handles the -get_time directive.
// Intended to query the wrapper/application for the current system time.
// The wrapper would typically respond with -set_time containing the timestamp.
// Not yet implemented — placeholder for future logic.
void Core::handle_get_time(const ArgParser&) {}

// Handles the -set_time directive.
// Intended to update the system’s clock with a new time provided by the wrapper.
// Could be used for synchronization across nodes.
// Not yet implemented — placeholder for future logic.
void Core::handle_set_time(const ArgParser&) {}


// --- BUILDERS FOR REPLIES ---


// --- Build ACK reply ---
// Purpose: Construct a short acknowledgment (-ack) message to send back to the original sender.
// This is used when we receive a message that requests acknowledgment, so the sender knows it arrived.
void Core::build_ack_args(const Message& message, char* out_buf, size_t max_len) {

    // Format of the ACK string we are building:
    //   -ack -from <our_node_id> -to <sender_id> -msg_id <sequence_number>
    //
    // node_id       = our own node's ID (string)
    // message.from  = the sender's ID (string)
    // message.id.sequence = unique message sequence number (integer)
    //
    // snprintf() safely writes formatted text into 'out_buf',
    // truncating if necessary so it never exceeds 'max_len' (including null terminator).
    snprintf(out_buf, max_len,
             "-ack -from %s -to %s -msg_id %u",
             node_id.c_str(),
             message.from.c_str(),
             static_cast<unsigned>(message.id.sequence));
}


// --- Build "received" reply ---
// Purpose: Construct a "-r" message to indicate that we have *received* a message.
// This can be used by the wrapper or UI to display incoming content, log it, or forward it.
void Core::build_received_args(const Message& message, char* out_buf, size_t max_len) {

    // Step 1: Create a buffer to hold the serialized (wire format) version of the original message.
    // The wire string is a compact representation that can be sent as plain text.
    char msg_buf[320];
    message.to_wire_string(msg_buf, sizeof(msg_buf));

    // Step 2: Format the "received" message string.
    //   -r = "received" directive
    //   -from <from> = ID of the original sender
    //   -data <wire_string> = serialized message payload
    //
    // This lets downstream systems know exactly what was received and from whom.
    snprintf(out_buf, max_len,
             "-r -from %s -data %s",
             message.from.c_str(),
             msg_buf);
}


// --- MESSAGE DEDUPLICATION ---

// --- Check if we've already seen this message ---
// Purpose: Prevents processing the same message twice.
// This is part of ViaText's deduplication system — useful in mesh networks where the same
// message may arrive from different paths.
//
// sequence = The unique message sequence number (id_t) extracted from the message header.
// Returns: true if this sequence number is already stored in our "recently seen" list.
bool Core::has_seen_message(id_t sequence) const {

    // Loop over all stored message IDs in our recent history buffer
    for (size_t i = 0; i < received_message_ids.size(); ++i) {

        // If we find a match, it means we've already processed this message
        if (received_message_ids[i] == sequence)
            return true;
    }

    // If no match found, this is a new message
    return false;
}



// --- Remember that we've seen this message ---
// Purpose: Adds a message's sequence number to the "recently seen" list
// so that if it arrives again, we can skip it.
//
// sequence = The unique message sequence number (id_t) to remember.
void Core::remember_message_id(id_t sequence) {

    // If our buffer of stored IDs is full, remove the oldest entry
    // This keeps the buffer size fixed (rolling history).
    if (received_message_ids.full())
        received_message_ids.erase(received_message_ids.begin());

    // Add the new sequence number to the end of the buffer
    received_message_ids.push_back(sequence);
}


} // namespace viatext
