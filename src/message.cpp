#include "message.hpp"
#include <sstream>

namespace viatext {

// Default constructor: sets all fields to their default (empty/zero) states.
// This creates a "blank" message ready for use.
Message::Message() : id(), from(), to(), data() {}

// Full-field constructor: initializes the message with all standard fields already set.
// Lets you create a complete message in one line by supplying the header, from, to, and data.
Message::Message(const MessageID& id_, const std::string& from_, const std::string& to_, const std::string& data_)
    : id(id_), from(from_), to(to_), data(data_) {}

// String constructor: takes a ViaText wire-format string and parses all fields from it.
// (e.g., "4F2B000131~NODE1~NODE2~Hello" becomes header, from, to, data)
Message::Message(const std::string& wire_str) : id(), from(), to(), data() {
    from_wire_string(wire_str); // Parse and fill fields from the protocol string
}

// Binary constructor: loads the message header fields from a packed byte array (typically received from wire).
// Only the header is loaded; from/to/data are left empty for manual population.
Message::Message(const uint8_t* buf, size_t len) : id(), from(), to(), data() {
    unpack(buf, len); // Parse and fill MessageID from the binary buffer
}

// clear(): Resets all fields to their initial/empty values.
// Useful when reusing a Message object or before parsing new data into it.
void Message::clear() {
    id = MessageID();   // Reset the header to zeros
    from.clear();       // Erase the sender field
    to.clear();         // Erase the recipient field
    data.clear();       // Erase the data payload
}


// Returns a constant reference to the internal MessageID header struct.
// Lets callers inspect or copy the routing, fragmentation, and flag data.
const MessageID& Message::get_id() const { return id; }

// Sets the entire MessageID header to a new value.
// Used when you want to replace the full header at once (e.g., after parsing).
void Message::set_id(const MessageID& id_) { id = id_; }

// Retrieves the sequence number (unique message identifier) from the header.
uint16_t Message::get_sequence() const { return id.sequence; }

// Sets the sequence number for this message (typically for new outbound messages).
void Message::set_sequence(uint16_t seq) { id.sequence = seq; }

// Gets the part number for this fragment (0 = first, 1 = second, etc.).
uint8_t Message::get_part() const { return id.part; }

// Sets the part number for this message fragment.
void Message::set_part(uint8_t part_) { id.part = part_; }

// Gets the total number of parts (fragments) for this message.
uint8_t Message::get_total() const { return id.total; }

// Sets the total number of parts (fragments) for this message.
void Message::set_total(uint8_t total_) { id.total = total_; }

// Retrieves the hop count (mesh rebroadcast/TTL) from the header.
uint8_t Message::get_hops() const { return id.hops; }

// Sets the hop count for this message. Only the lower 4 bits are stored (0-15).
void Message::set_hops(uint8_t hops_) { id.hops = hops_ & 0x0F; }

// Gets the raw 4-bit flags field (for ACK, encryption, etc.) from the header.
uint8_t Message::get_flags() const { return id.flags; }

// Sets the raw 4-bit flags field (see protocol docs for meaning).
void Message::set_flags(uint8_t flags_) { id.flags = flags_ & 0x0F; }

// Returns true if this message is requesting an acknowledgment (ACK) reply.
// Looks up the relevant flag bit in the header.
bool Message::requests_acknowledgment() const { return id.requests_acknowledgment(); }

// Sets or clears the "request acknowledgment" flag in the header.
void Message::set_request_acknowledgment(bool enable) { id.set_request_acknowledgment(enable); }

// Returns true if this message is an acknowledgment (ACK) reply.
bool Message::is_acknowledgment() const { return id.is_acknowledgment(); }

// Sets or clears the "acknowledgment" flag (used for ACK replies).
void Message::set_is_acknowledgment(bool enable) { id.set_is_acknowledgment(enable); }

// Returns true if the message payload is encrypted (flag set).
bool Message::is_encrypted() const { return id.is_encrypted(); }

// Sets or clears the "encrypted" flag (marking payload as encrypted).
void Message::set_is_encrypted(bool enable) { id.set_is_encrypted(enable); }

// Returns a reference to the sender's callsign or node ID ("from" field).
const std::string& Message::get_from() const { return from; }

// Sets the sender's callsign or node ID ("from" field).
void Message::set_from(const std::string& from_) { from = from_; }

// Returns a reference to the recipient's callsign or node ID ("to" field).
const std::string& Message::get_to() const { return to; }

// Sets the recipient's callsign or node ID ("to" field).
void Message::set_to(const std::string& to_) { to = to_; }

// Returns a reference to the main message data/payload ("data" field).
const std::string& Message::get_data() const { return data; }

// Sets the main message data/payload ("data" field).
void Message::set_data(const std::string& data_) { data = data_; }


// Returns true if this message is a fragment (i.e., it's part of a larger multi-part message).
// This is determined by checking if the total part count is greater than 1.
// If true, you need to assemble fragments to get the full original message.
bool Message::is_fragmented() const {
    return id.total > 1;
}

// Returns true if this message is a complete, single-part message (not fragmented).
// This is true only if it's the first part (part == 0) and there is only one total part (total == 1).
// Indicates a self-contained message that does not require any reassembly.
bool Message::is_complete() const {
    return id.part == 0 && id.total == 1;
}

// Compares this message to another message for equality.
// Returns true only if all key fields match exactly: header values, sender, recipient, and data.
// Useful for deduplication, testing, and verifying that two messages are identical.
bool Message::operator==(const Message& other) const {
    // All fields must match for messages to be considered equal:
    // - Same message sequence number
    // - Same fragment part and total count
    // - Same sender ("from") and recipient ("to")
    // - Same data payload
    return id.sequence == other.id.sequence &&
           id.part == other.id.part &&
           id.total == other.id.total &&
           from == other.from &&
           to == other.to &&
           data == other.data;
}

// Converts this message to the official ViaText wire format as a single tilde-delimited string.
// The result can be sent directly over LoRa, serial, or any ViaText-compatible transport.
// Format: [HEADER_HEX]~[FROM]~[TO]~[DATA]
std::string Message::to_wire_string() const {
    std::ostringstream oss;      // Create a string stream to assemble the final string.

    char hexbuf[11];             // Temporary buffer to hold the 10-character hex header (+1 for null terminator).
    uint8_t packed_id[5];        // 5-byte array for the packed header.

    id.pack(packed_id);          // Fill packed_id with the MessageID header (5 bytes: sequence, part, total, hops, flags).

    // Convert each header byte to its two-digit hexadecimal representation.
    // Loop through each byte of the packed header.
    for (int i = 0; i < 5; ++i) {
        // Write two hex digits for each byte into the correct spot in hexbuf.
        // "%02X" ensures uppercase and always two characters (pads with zero if needed).
        snprintf(hexbuf + i * 2, 3, "%02X", packed_id[i]);
    }

    // Build the wire string by joining all parts with '~' as delimiter.
    // [hex header] ~ [from] ~ [to] ~ [data]
    oss << hexbuf << "~" << from << "~" << to << "~" << data;

    // Return the assembled protocol string, ready for sending over the network.
    return oss.str();
}

// Parses a ViaText wire-format string ("HEADER~FROM~TO~DATA") and fills all fields of this message.
// Resets the message to empty first, then splits the input and unpacks each field as needed.
void Message::from_wire_string(const std::string& wire_str) {
    clear(); // Start fresh: remove any existing data in this message.

    // Find the first delimiter (~), which separates the header from the rest.
    size_t pos1 = wire_str.find('~');
    if (pos1 == std::string::npos) return; // If not found, input is invalid; leave message empty.

    // Extract the header hex string (should be 10 characters for 5 bytes).
    std::string idstr = wire_str.substr(0, pos1);
    uint8_t packed_id[5] = {0}; // Temporary buffer for header bytes.

    // If the header string is the correct length, parse each pair of hex digits into a byte.
    if (idstr.size() == 10) {
        for (int i = 0; i < 5; ++i) {
            // Get two characters for this byte (e.g., "4F", "2B", ...).
            std::string byte_str = idstr.substr(i * 2, 2);
            // Convert the two-character hex string to an 8-bit unsigned value.
            packed_id[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        }
        // Fill the MessageID struct with the unpacked header values.
        id.unpack(packed_id, 5);
    }

    // Find the second delimiter (~) to get the 'from' field.
    size_t pos2 = wire_str.find('~', pos1 + 1);
    if (pos2 == std::string::npos) return; // Malformed input: missing 'from' or 'to'.

    // Extract the sender's callsign (from the string after first '~' up to second '~').
    from = wire_str.substr(pos1 + 1, pos2 - pos1 - 1);

    // Find the third delimiter (~) to get the 'to' field.
    size_t pos3 = wire_str.find('~', pos2 + 1);
    if (pos3 == std::string::npos) return; // Malformed input: missing data field.

    // Extract the recipient's callsign.
    to = wire_str.substr(pos2 + 1, pos3 - pos2 - 1);

    // The remainder of the string (after the third '~') is the message data.
    data = wire_str.substr(pos3 + 1);
}


// Packs the message header (MessageID) into a binary buffer for transmission or storage.
// Only the first 5 bytes of out_buf are written (ViaText header size).
// If the buffer is too small, nothing is written (protects against overflow).
void Message::pack(uint8_t* out_buf, size_t buf_len) const {
    if (buf_len < 5) return;     // Require at least 5 bytes for the header.
    id.pack(out_buf);            // Write the 5-byte MessageID struct to the buffer.
}

// Unpacks the message header (MessageID) from a binary buffer.
// Reads the first 5 bytes of in_buf and fills the MessageID fields accordingly.
// If the buffer is too small, nothing is changed.
void Message::unpack(const uint8_t* in_buf, size_t len) {
    if (len < 5) return;         // Require at least 5 bytes for the header.
    id.unpack(in_buf, 5);        // Read and decode the 5-byte header into MessageID fields.
}


// Returns a human-readable string describing the contents of this message.
// Useful for debugging, logging, or displaying the message to users or developers.
std::string Message::to_string() const {
    std::ostringstream oss; // String stream to build the formatted output.

    // Output each key field (header, from, to, data) in a structured, readable way.
    oss << "[SEQ:" << id.sequence              // Print sequence number.
        << " PART:" << int(id.part)            // Print part number (as integer).
        << "/" << int(id.total)                // Print total part count (as integer).
        << " HOPS:" << int(id.hops)            // Print hop count.
        << " FLAGS:0x" << std::hex << int(id.flags) << std::dec // Print flags in hex.
        << "] FROM:" << from                   // Print sender/callsign.
        << " TO:" << to                        // Print recipient/callsign.
        << " DATA:" << data;                   // Print the main message data.

    return oss.str(); // Return the constructed string.
}
}
