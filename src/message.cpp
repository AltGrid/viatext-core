/**
 * @file message.cpp
 * @brief Implementation of the ViaText Message class.
 *
 * Refer to `message.hpp` for complete code-level documentation.
 * Refer to `message.md` for conceptual overview, design intent, and AI collaboration blueprint.
 */
#include "viatext/message.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include <stdio.h>
#include <string.h>

namespace viatext {

// --- Default constructor: zero/init all fields
Message::Message()
    : id(), from(), to(), data(), error(3) // error=3 means empty
{
    // Constructs a message object in a safe, inert state.
    // All fields are default-constructed:
    // - id       → MessageID with sequence = 0
    // - from/to  → empty ETL strings (max 6 chars)
    // - data     → empty TextFragments container (8 × 32B default)
    // - error    → 3, which denotes "empty/uninitialized" in ViaText error codes

    // This constructor is used when preparing a blank message
    // to be populated later via setters or deserialization.
}

// --- Full-field constructor (all const char* for MCU/ETL)
Message::Message(const MessageID& id_, const char* from_, const char* to_, const char* data_)
    : id(id_), from(from_), to(to_), data(), error(0)
{
    // Constructs a fully populated message using the provided header and string fields.

    // Note: `from`, `to`, and `data` are passed as `const char*` to remain compatible with
    // microcontroller environments and avoid dynamic allocation.

    // These calls ensure:
    // - Strings are properly truncated if needed (via assign)
    // - Data is fragmented safely into ETL blocks
    // - Any error in fragmentation is tracked in the `error` field

    set_from(from_);  // Safely assign sender field (max 6 chars)
    set_to(to_);      // Safely assign recipient field (max 6 chars)
    set_data(data_);  // Split and store message payload (max 256B total)

    // If set_data encounters overflow, `error` will be set to 2 automatically.
}


// --- Parse from wire-format string (tilde-delimited)
Message::Message(const char* wire_str)
    : id(), from(), to(), data(), error(3) // Start in "empty" error state
{
    // This constructor attempts to parse a complete message from a serialized wire-format string.
    //
    // Format expected: "HEADER~FROM~TO~DATA"
    // Example: "01AB2300FF~NODE1~NODE2~temperature=24.5"
    //
    // Each field is separated by a tilde '~', and the HEADER is a 10-character hex string
    // representing 5 packed bytes (MessageID).
    //
    // The format is compact, human-readable, and MCU-friendly — ideal for LoRa, Serial, or Sneakernet use.

    // Attempt to parse immediately — result stored in `error` field:
    // - Success sets `error = 0`
    // - Failure sets `error = 1` (parse error) or leaves `error = 3` (empty)
    from_wire_string(wire_str);
}

// --- Setters

/**
 * @brief Set the MessageID (header) for this message.
 * @param id_ A fully constructed MessageID object containing sequence, part, total, etc.
 *
 * Used in cases where the ID is assigned externally (e.g., before transmission),
 * allowing this Message object to receive or prepare for routing.
 */
void Message::set_id(const MessageID& id_) {
    id = id_;
}

/**
 * @brief Set the sender ID field ("from") from a C-string.
 * @param from_ Null-terminated string, max length FROM_LEN (default: 6).
 *
 * Clears any existing content, then assigns characters up to the allowed limit.
 * This ensures safety on microcontrollers without risking overflow.
 *
 * If `from_` is nullptr, the field is cleared to empty.
 *
 * Example: `set_from("NODE1")` → from = "NODE1"
 */
void Message::set_from(const char* from_) {
    from.clear();  // Ensure clean state
    if (from_) {
        from.assign(from_, FROM_LEN); // ETL-safe bounded copy
    }
}

/**
 * @brief Set the recipient ID field ("to") from a C-string.
 * @param to_ Null-terminated string, max length TO_LEN (default: 6).
 *
 * Clears any previous content, then assigns the input string up to the allowed length.
 * Ensures overflow-safe assignment for constrained environments (e.g., Arduino/ESP32).
 *
 * If `to_` is nullptr, the field is cleared to empty.
 *
 * Example: `set_to("NODE2")` → to = "NODE2"
 */
void Message::set_to(const char* to_) {
    to.clear();  // Remove any existing data
    if (to_) {
        to.assign(to_, TO_LEN);  // ETL-bounded assignment
    }
}

/**
 * @brief Set the message payload using a raw C-string.
 * @param data_ Null-terminated string containing the full payload to be fragmented.
 *
 * The string is internally split into fixed-size text fragments using the `TextFragments` container.
 * This allows the message to be transmitted over constrained networks like LoRa or serial,
 * where packet size is limited (e.g., 32 bytes per fragment, 8 total fragments).
 *
 * Sets `error = 2` (overflow) if the input string exceeds the available fragment capacity.
 * Otherwise, resets `error = 0` to indicate success.
 *
 * Example:
 * ```cpp
 * set_data("temperature=25.3&unit=C");
 * ```
 */
void Message::set_data(const char* data_) {
    data.set(data_);  // Fragment and store the input string
    if (data.error != 0) {
        error = 2;  // Signal overflow or parse error
    } else {
        error = 0;  // All good
    }
}


// --- Getters

/**
 * @brief Get a constant reference to the message's routing ID.
 * 
 * The MessageID contains the compact 5-byte header used for routing, fragmentation,
 * and deduplication. This method provides read-only access to the identifier.
 *
 * Used by routers, dispatchers, or loggers that inspect metadata without modifying it.
 *
 * @return const MessageID& — immutable reference to internal `id`.
 */
const MessageID& Message::get_id() const { return id; }

/**
 * @brief Get the "from" field (sender node ID) as a bounded ETL string.
 * 
 * Returns a constant reference to the sender string (max length FROM_LEN).
 * This string identifies the original sender of the message and is used in delivery confirmation,
 * acknowledgments, and message tracing.
 *
 * @return const etl::string<FROM_LEN>& — immutable sender string.
 */
const etl::string<FROM_LEN>& Message::get_from() const { return from; }

/**
 * @brief Get the "to" field (recipient node ID) as a bounded ETL string.
 * 
 * Returns a constant reference to the destination string (max length TO_LEN).
 * Used by local routing logic to determine whether the current node is the intended recipient.
 *
 * @return const etl::string<TO_LEN>& — immutable recipient string.
 */
const etl::string<TO_LEN>& Message::get_to() const { return to; }

/**
 * @brief Get the message payload (text) as a fixed-fragment container.
 * 
 * Returns a reference to the `TextFragments` object, which stores the full message body
 * across 8 × 32-byte chunks (by default). Designed to be used safely in constrained systems.
 * 
 * The result can be iterated over, flattened, or passed directly to rendering, transmission,
 * or further protocol parsing systems like ArgParser.
 *
 * @return const TextFragments<DATA_FRAGS, FRAG_SIZE>& — immutable payload data container.
 */
const TextFragments<DATA_FRAGS, FRAG_SIZE>& Message::get_data() const { return data; }


/**
 * @brief Serialize the message into a compact wire-format string.
 * 
 * Converts the message fields into a single flat ASCII string for transmission.
 * The format is: "HEADER~FROM~TO~DATA", where HEADER is a 10-character hex-encoded
 * representation of the 5-byte MessageID.
 * 
 * This format is human-readable, MCU-safe, and suitable for LoRa, serial, or sneakernet.
 * 
 * @param[out] out      The destination char buffer to receive the encoded message.
 * @param[in]  max_len  The maximum number of bytes to write into `out`, including null terminator.
 */
void Message::to_wire_string(char* out, size_t max_len) const {
    // Prepare a buffer for the header string (5 bytes * 2 hex digits = 10 chars + null terminator)
    char hexbuf[11] = {0};

    // Step 1: Pack the binary ID into a 5-byte array
    uint8_t packed_id[5];
    id.pack(packed_id);

    // Step 2: Convert each byte to a 2-digit uppercase hex string (e.g. 0x3F → "3F")
    for (int i = 0; i < 5; ++i) {
        snprintf(hexbuf + i * 2, 3, "%02X", packed_id[i]);
        // Note: offset advances 2 chars each time (hex string per byte)
        // snprintf writes up to 2 + null → safe at 3
    }

    // Step 3: Join all data fragments into a flat string
    // This concatenates all message fragments into one clean block
    etl::string<DATA_FRAGS * FRAG_SIZE> flat;
    for (uint8_t i = 0; i < data.used_fragments; ++i) {
        flat.append(data.fragments[i]);  // Append each 32-byte max fragment
    }

    // Step 4: Compose final wire-format string using tilde (~) delimiters
    // Final format: HEADER~FROM~TO~DATA
    snprintf(out, max_len, "%s~%s~%s~%s", 
        hexbuf,            // e.g. "01A2B3C4D5"
        from.c_str(),      // sender node ID
        to.c_str(),        // destination node ID
        flat.c_str());     // combined data payload
}


/**
 * @brief Parse a wire-format string into a Message object.
 * 
 * Expects input in the form: "HEADER~FROM~TO~DATA"
 * Where:
 *   - HEADER is a 10-character hex string representing the packed 5-byte MessageID
 *   - FROM and TO are short alphanumeric node names (e.g., "NODE1", "BCAST")
 *   - DATA is a flat string (can be re-fragmented internally)
 * 
 * This method mutates the `Message` fields directly and sets the `error` field if parsing fails.
 * 
 * @param wire_str A tilde-delimited C-string to decode.
 * @return true if the message was successfully parsed and is structurally valid.
 */
bool Message::from_wire_string(const char* wire_str) {
    clear();         // Reset message state before parsing
    error = 3;       // Assume empty/error until proven otherwise

    // ------------------------------------------------------------------------
    // Step 1: Split the input string into 4 fields using the tilde (~) delimiter
    // We expect exactly 3 tildes (4 parts): HEADER, FROM, TO, DATA
    const char* fields[4] = {nullptr, nullptr, nullptr, nullptr};
    int field_idx = 0;
    fields[0] = wire_str;  // The first field always starts at beginning

    for (const char* p = wire_str; *p && field_idx < 3; ++p) {
        if (*p == '~') {
            fields[++field_idx] = p + 1;         // Set next field pointer
            *((char*)p) = '\0';                  // Null-terminate current field in place
            // NOTE: Modifying const char* is usually unsafe, but here it's allowed because:
            // - We're in a controlled ETL/MCU environment
            // - We assume wire_str was passed as a modifiable buffer
        }
    }

    if (field_idx < 3) {
        error = 1;  // Too few fields → malformed message
        return false;
    }

    // ------------------------------------------------------------------------
    // Step 2: Parse the HEADER field (5 bytes as 10-character hex)
    // Validate length before decoding
    if (strlen(fields[0]) == 10) {
        uint8_t packed_id[5] = {0};
        for (int i = 0; i < 5; ++i) {
            // Extract 2 characters at a time from hex string
            char byte_str[3] = {fields[0][i * 2], fields[0][i * 2 + 1], 0};
            packed_id[i] = (uint8_t)strtoul(byte_str, nullptr, 16);  // Convert hex → byte
        }
        id.unpack(packed_id, 5);  // Populate MessageID struct from binary
    } else {
        error = 1;  // Invalid header length (should be exactly 10)
        return false;
    }

    // ------------------------------------------------------------------------
    // Step 3: Load remaining fields into the message
    // Truncation safety is handled by each set_* function
    set_from(fields[1]);  // e.g. "NODE1"
    set_to(fields[2]);    // e.g. "NODE2"
    set_data(fields[3]);  // Will split into fragments automatically

    // If we made it here, all fields parsed correctly
    error = 0;
    return true;
}


// --- Validity check
// --- Validity check

/**
 * @brief Check whether the message has all required fields filled.
 *
 * A message is considered valid if:
 *   - `from` field is non-empty
 *   - `to` field is non-empty
 *   - At least one data fragment is present
 *   - The MessageID has a non-zero sequence number (used to uniquely identify messages)
 *
 * This does NOT guarantee the message was parsed correctly — only that the fields are non-trivial.
 * Useful as a high-level sanity check before transmission or processing.
 *
 * @return true if the message appears structurally valid.
 */
bool Message::is_valid() const {
    return (from.size() > 0 && to.size() > 0 && data.used_fragments > 0 && id.sequence > 0);
}

/**
 * @brief Check whether the message is part of a multi-part fragment.
 *
 * This indicates that the message is one fragment in a larger payload.
 * Used in reassembly logic to determine if message chunking is in play.
 *
 * Logic:
 *   - If the total expected parts > 1, this message is considered fragmented.
 *
 * @return true if message is part of a fragment series.
 */
bool Message::is_fragmented() const {
    return id.total > 1;
}

/**
 * @brief Check whether the message is a complete, unfragmented unit.
 *
 * Useful for determining if a message can be handled directly without reassembly.
 *
 * Logic:
 *   - A complete message must have:
 *     - `part == 0` (it's the first and only part)
 *     - `total == 1` (only 1 part expected)
 *
 * This is true for all simple messages that fit into one packet.
 *
 * @return true if message is self-contained and requires no reassembly.
 */
bool Message::is_complete() const {
    return id.part == 0 && id.total == 1;
}


// --- Human-readable (debug) string

/**
 * @brief Formats the message into a readable debug string.
 *
 * This function is intended for developers, logs, or shell outputs — NOT for wire transmission.
 * It outputs a simple one-line summary of the message:
 *
 *   "<ID> FROM:<from> TO:<to> DATA:<first fragment>"
 *
 * Notes:
 *   - Only the first data fragment is shown here for brevity.
 *   - Uses MessageID::to_string() to format the ID into ASCII.
 *   - Truncates output at `max_len` characters to prevent overflows.
 *
 * Example output:
 *   "SEQ=103 TYPE=2 FLAGS=01 FROM:NODE1 TO:NODE2 DATA:hello world"
 *
 * @param out      Preallocated char buffer to write to.
 * @param max_len  Maximum number of bytes to write (including null terminator).
 */
void Message::to_string(char* out, size_t max_len) const {
    char id_str[64];
    id.to_string(id_str, sizeof(id_str));
    snprintf(out, max_len, "%s FROM:%s TO:%s DATA:%s", id_str, from.c_str(), to.c_str(), data.fragments[0].c_str());
}

// --- Clear all fields to default state

/**
 * @brief Resets the entire Message to its initial state.
 *
 * All fields are cleared or re-initialized:
 *   - MessageID set to default (zeroed)
 *   - `from` and `to` cleared
 *   - Data fragments cleared
 *   - `error` set to 3 (empty state)
 *
 * This is useful for reusing Message objects in loops or after parsing fails.
 */
void Message::clear() {
    id = MessageID();       // Reset ID (default constructor)
    from.clear();           // Clear sender
    to.clear();             // Clear recipient
    data.clear();           // Clear all data fragments
    error = 3;              // Set to "empty message" status
}


} // namespace viatext
