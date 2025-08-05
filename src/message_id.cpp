#include "message_id.hpp"
#include <stdio.h> // For snprintf (string formatting)
#include <string.h> //for strlen
namespace viatext {

// -----------------------------------------------------------------------------
// Default constructor
// Sets all fields (sequence, part, total, hops, flags) to zero.
// -----------------------------------------------------------------------------
MessageID::MessageID()
    : sequence(0), part(0), total(0), hops(0), flags(0)
{}


// 1. Constructor: Populate from a 5-byte (40-bit) integer, big-endian order
MessageID::MessageID(uint64_t five_byte_value) {
    uint8_t buf[5]; // Temporary buffer to hold 5 bytes

    // We want to break the 40-bit integer into 5 bytes.
    // Each line shifts the value to bring the correct byte into the least significant position, 
    // then masks off everything else except the lowest byte (with & 0xFF).
    // The most significant byte (leftmost in hex) is stored first (big-endian/network order).

    buf[0] = (five_byte_value >> 32) & 0xFF; // Extract byte 1 (highest, leftmost in hex)
    buf[1] = (five_byte_value >> 24) & 0xFF; // Extract byte 2
    buf[2] = (five_byte_value >> 16) & 0xFF; // Extract byte 3
    buf[3] = (five_byte_value >> 8)  & 0xFF; // Extract byte 4
    buf[4] =  five_byte_value        & 0xFF; // Extract byte 5 (lowest, rightmost in hex)

    // Now we have the message header as a byte array, ready for parsing
    unpack(buf, 5); // This will decode sequence, part, total, hops, and flags
}

// 2. Constructor: Populate from hex string ("0x4F2B000131" or "4F2B000131")
MessageID::MessageID(const char* hex_str) {
    uint8_t buf[5] = {0}; // Temporary buffer for parsed bytes

    // Try to parse the input string into 5 bytes.
    // hex_str_to_bytes handles optional "0x" prefix and validates length/content.
    if (hex_str_to_bytes(hex_str, buf, 5)) {
        // If parsing succeeds, interpret the result as a message header
        unpack(buf, 5);
    } else {
        // If parsing fails (bad string/length/characters), set all fields to zero as a safe fallback
        sequence = 0;
        part = 0;
        total = 0;
        hops = 0;
        flags = 0;
    }
}


// -----------------------------------------------------------------------------
// Constructor: Initialize MessageID from a packed byte array
// Reads all fields from the provided buffer using the unpack() function.
// -----------------------------------------------------------------------------
MessageID::MessageID(const uint8_t* data, size_t len) {
    // Calls unpack() to extract all fields from the buffer.
    unpack(data, len);
}

// -----------------------------------------------------------------------------
// Constructor: Initialize MessageID from individual field values
// -----------------------------------------------------------------------------
MessageID::MessageID(uint16_t seq, uint8_t p, uint8_t tot, uint8_t h, uint8_t f)
    : sequence(seq), part(p), total(tot), hops(h & 0x0F), flags(f & 0x0F)
    // Only lower 4 bits of hops and flags are used (masked with 0x0F)
{}

MessageID::MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops,
                     bool request_acknowledge, bool is_acknowledgment, 
                     bool is_encrypted, bool unused)
    : sequence(sequence), part(part), total(total), hops(hops & 0x0F), flags(0)
{
    // Set flag bits according to input booleans
    if (request_acknowledge) set_request_acknowledgment(request_acknowledge); // Bit 0
    if (is_acknowledgment)   set_is_acknowledgment(is_acknowledgment);        // Bit 1
    if (is_encrypted)        set_is_encrypted(is_encrypted);                  // Bit 2
    if (unused)              flags |= 0x8;    // Bit 3 (unused/reserved)
}


// -----------------------------------------------------------------------------
// pack() — Packs all fields into a 5-byte array (LoRa message header format)
// The packed array will be used for transmission/storage.
// Format: [0]=sequence high byte, [1]=sequence low byte, [2]=part, [3]=total, [4]=hops+flags
// -----------------------------------------------------------------------------
void MessageID::pack(uint8_t* out_buf) const {
    // Sequence number is split into two bytes (big-endian format)
    out_buf[0] = (sequence >> 8) & 0xFF; // High byte of sequence
    out_buf[1] = sequence & 0xFF;        // Low byte of sequence
    out_buf[2] = part;                   // Part number
    out_buf[3] = total;                  // Total parts
    // Combine hops (upper 4 bits) and flags (lower 4 bits) into one byte
    out_buf[4] = ((hops & 0x0F) << 4) | (flags & 0x0F);
}

// -----------------------------------------------------------------------------
// unpack() — Parses a packed 5-byte (or 5-byte) array and populates all fields
// If input is too short (<5 bytes), all fields are set to zero.
// -----------------------------------------------------------------------------
void MessageID::unpack(const uint8_t* in_buf, size_t len) {
    if (len < 5) {
        // Not enough data — clear all fields
        sequence = 0;
        part = 0;
        total = 0;
        hops = 0;
        flags = 0;
        return;
    }
    // Sequence number (16 bits): combine high and low bytes
    sequence = (in_buf[0] << 8) | in_buf[1];
    part     = in_buf[2];                   // Part number
    total    = in_buf[3];                   // Total number of parts
    // Unpack hops and flags from a single byte
    hops     = (in_buf[4] >> 4) & 0x0F;     // Hops: upper 4 bits
    flags    = in_buf[4] & 0x0F;            // Flags: lower 4 bits
}

// -----------------------------------------------------------------------------
// to_string() — Formats the MessageID fields as a human-readable C string
// Example: "SEQ:28 PART:5/7 HOPS:10 FLAGS:0xC"
// Useful for logging, debugging, or CLI output.
// -----------------------------------------------------------------------------
void MessageID::to_string(char* out, size_t max_len) const {
    // snprintf safely writes up to max_len-1 characters, always null-terminated
    snprintf(out, max_len, "SEQ:%u PART:%u/%u HOPS:%u FLAGS:0x%X",
        sequence, part, total, hops, flags);
}

// -----------------------------------------------------------------------------
// Returns true if the "request acknowledgment" flag (bit 0) is set.
// This means the sender wants the recipient to reply with an ACK message.
// -----------------------------------------------------------------------------
bool MessageID::requests_acknowledgment() const {
    // Bit 0 (0x1): If set, ACK is requested.
    // Use bitwise AND to isolate bit 0; if the result is nonzero, it's set.
    return (flags & 0x1) != 0;
}

// -----------------------------------------------------------------------------
// Returns true if the "acknowledgment" flag (bit 1) is set.
// This means the message itself is an ACK reply.
// -----------------------------------------------------------------------------
bool MessageID::is_acknowledgment() const {
    // Bit 1 (0x2): If set, this is an ACK message.
    // Use bitwise AND with 0x2 to check bit 1.
    return (flags & 0x2) != 0;
}

// -----------------------------------------------------------------------------
// Returns true if the "encrypted" flag (bit 2) is set.
// This means the payload of the message is encrypted.
// -----------------------------------------------------------------------------
bool MessageID::is_encrypted() const {
    // Bit 2 (0x4): If set, payload is encrypted.
    // Use bitwise AND with 0x4 to check bit 2.
    return (flags & 0x4) != 0;
}

// -----------------------------------------------------------------------------
// Sets or clears the "request acknowledgment" flag (bit 0).
// If enable is true, sets bit 0 (sender requests ACK). If false, clears it.
void MessageID::set_request_acknowledgment(bool enable) {
    if (enable) {
        flags |= 0x1;   // Set bit 0 using bitwise OR
    } else {
        flags &= ~0x1;  // Clear bit 0 using bitwise AND with inverted mask
    }
}

// -----------------------------------------------------------------------------
// Sets or clears the "acknowledgment" flag (bit 1).
// If enable is true, sets bit 1 (this is an ACK reply). If false, clears it.
void MessageID::set_is_acknowledgment(bool enable) {
    if (enable) {
        flags |= 0x2;   // Set bit 1 using bitwise OR
    } else {
        flags &= ~0x2;  // Clear bit 1 using bitwise AND with inverted mask
    }
}

// -----------------------------------------------------------------------------
// Sets or clears the "encrypted" flag (bit 2).
// If enable is true, sets bit 2 (payload is encrypted). If false, clears it.
void MessageID::set_is_encrypted(bool enable) {
    if (enable) {
        flags |= 0x4;   // Set bit 2 using bitwise OR
    } else {
        flags &= ~0x4;  // Clear bit 2 using bitwise AND with inverted mask
    }
}


// --- PRIVATE HELPERS ---

// Convert a single hex character ('0'-'9', 'A'-'F', 'a'-'f') to its numeric value (0-15).
// Returns true if the character is valid, false if not.
bool MessageID::hex_char_to_val(char c, uint8_t& out) {
    // Check if the character is a decimal digit (0-9)
    if ('0' <= c && c <= '9') {
        out = c - '0';        // e.g. '4' - '0' == 4
        return true;
    }
    // Check if the character is an uppercase letter (A-F)
    if ('A' <= c && c <= 'F') {
        out = c - 'A' + 10;   // e.g. 'C' - 'A' == 2, 2 + 10 == 12
        return true;
    }
    // Check if the character is a lowercase letter (a-f)
    if ('a' <= c && c <= 'f') {
        out = c - 'a' + 10;   // e.g. 'e' - 'a' == 4, 4 + 10 == 14
        return true;
    }
    // Character is not a valid hex digit
    return false;
}

// Converts a hex string (optionally prefixed with "0x" or "0X") into a byte array.
// Fills out_bytes with the result if successful.
// bytes_needed is the expected number of bytes to extract (should be 5 for MessageID).
// Returns true on success, false if the input is malformed.
bool MessageID::hex_str_to_bytes(const char* str, uint8_t* out_bytes, size_t bytes_needed) {
    size_t len = strlen(str); // Get the length of the input string

    // Check if the string starts with "0x" or "0X"
    if (len >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        str += 2;     // Move the pointer past the prefix
        len -= 2;     // Reduce the length accordingly
    }

    // The string must now be exactly twice as long as the number of bytes we need (2 hex chars per byte)
    if (len != bytes_needed * 2)
        return false; // Input is the wrong length—fail

    // Loop through each byte to build
    for (size_t i = 0; i < bytes_needed; ++i) {
        uint8_t hi = 0, lo = 0; // Will hold high and low nibble (half-bytes)
        // Convert the first hex digit of the pair (high nibble)
        if (!hex_char_to_val(str[i*2], hi)) return false;   // Fail if not valid
        // Convert the second hex digit of the pair (low nibble)
        if (!hex_char_to_val(str[i*2+1], lo)) return false; // Fail if not valid
        // Combine the two nibbles into one byte (e.g. 'A5' → (0xA << 4) | 0x5 == 0xA5)
        out_bytes[i] = (hi << 4) | lo;
    }
    // All digits valid, output array filled
    return true;
}


} // namespace viatext
