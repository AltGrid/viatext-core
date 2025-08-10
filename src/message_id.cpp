// -----------------------------------------------------------------------------
// @file message_id.cpp
// @brief Implementation of the MessageID struct used in ViaText headers.
//
// This file provides the full implementation for the MessageID class, which
// represents a compact 5-byte routing and control header used in the ViaText
// protocol.
//
// Implemented methods include:
// - Constructors (default, from buffer, hex string, raw fields)
// - Packing and unpacking to/from raw byte arrays
// - Human-readable string conversion (embedded-safe)
// - Bitwise accessors for ACK/encryption flags
// - Static helpers for hex string parsing
//
// @note All methods are safe for cross-platform use (Linux, Arduino, etc.)
//       and avoid heap allocation.
//
// @author Leo
// @author ChatGPT
// -----------------------------------------------------------------------------
#include "viatext/message_id.hpp"

namespace viatext {

// =============================================================================
// Constructors
// =============================================================================

// Default constructor.
// Initializes all fields (sequence, part, total, hops, flags) to zero.
// Useful as a neutral placeholder or for zero-initializing before unpacking.
MessageID::MessageID() : sequence(0), part(0), total(0), hops(0), flags(0) {}

// Constructor from a packed 5-byte integer (big-endian format)

MessageID::MessageID(uint64_t five_byte_value) {
    
    // Create a temporary 5-byte buffer to hold each byte of the input value
    uint8_t buf[5];
    
    // Extract the highest byte (bits 32–39) and store in buf[0]
    buf[0] = (five_byte_value >> 32) & 0xFF;
    
    // Extract next highest byte (bits 24–31) → buf[1]
    buf[1] = (five_byte_value >> 24) & 0xFF;
    
    // Extract middle byte (bits 16–23) → buf[2]
    buf[2] = (five_byte_value >> 16) & 0xFF;
    
    // Extract second-lowest byte (bits 8–15) → buf[3]
    buf[3] = (five_byte_value >> 8)  & 0xFF;

    // Extract lowest byte (bits 0–7) → buf[4]
    buf[4] =  five_byte_value        & 0xFF;

    // Decode these 5 bytes into the message fields using existing unpack logic
    unpack(buf, 5);
}

// Constructor from a 10-character hex string (e.g., "4F2B000131")
MessageID::MessageID(const char* hex_str) {
    // Initialize a 5-byte buffer to zero — will hold parsed bytes if successful
    uint8_t buf[5] = {0};
    // Try to parse the hex string into the buffer
    if (hex_str_to_bytes(hex_str, buf, 5)) {
        // If parsing succeeds, extract the fields from the buffer
        unpack(buf, 5);
    } else {
        // If parsing fails, safely zero all fields
        sequence = part = total = hops = flags = 0;
    }
}

// Constructor from raw byte array (usually from a received packet)
MessageID::MessageID(const uint8_t* data, size_t len) {
    // Delegate directly to unpack(): safely parses up to 5 bytes into fields
    unpack(data, len);
}

// Constructor from raw field values
MessageID::MessageID(uint16_t seq, uint8_t p, uint8_t tot, uint8_t h, uint8_t f)
    // Assign sequence, part, and total directly
    : sequence(seq), part(p), total(tot), 
    // Use only the lower 4 bits for hops (mask with 0x0F to enforce 4-bit limit)
    hops(h & 0x0F), 
    // Use only the lower 4 bits for flags (mask with 0x0F to enforce 4-bit limit)
    flags(f & 0x0F) {}

// Constructor with raw fields + optional flags for ACK and encryption
MessageID::MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops,
                     bool request_acknowledge, bool is_acknowledgment,
                     bool is_encrypted, bool unused)
    : sequence(sequence), part(part), total(total), hops(hops & 0x0F), flags(0)
{
    // Set flag bit 0 if ACK is requested
    if (request_acknowledge) set_request_acknowledgment(true);
    // Set flag bit 1 if this is an ACK message
    if (is_acknowledgment)   set_is_acknowledgment(true);
    // Set flag bit 2 if the message is encrypted
    if (is_encrypted)        set_is_encrypted(true);
    // Set flag bit 3 if explicitly requested (reserved/future use)
    if (unused)              flags |= 0x8;
}

// =============================================================================
// Packing & Unpacking
// =============================================================================

// Convert internal fields into a 5-byte buffer for transmission
void MessageID::pack(uint8_t* out_buf) const {
    // Store the high byte of the 16-bit sequence number (big-endian)
    out_buf[0] = (sequence >> 8) & 0xFF;

    // Store the low byte of the sequence number
    out_buf[1] = sequence & 0xFF;

    // Store the part number (which fragment this is)
    out_buf[2] = part;

    // Store the total number of parts in the full message
    out_buf[3] = total;

    // Combine hops (upper 4 bits) and flags (lower 4 bits) into one byte
    out_buf[4] = ((hops & 0x0F) << 4) | (flags & 0x0F);
}

// Convert a 5-byte buffer into internal fields for routing and control
void MessageID::unpack(const uint8_t* in_buf, size_t len) {
    // If the buffer is too short, clear all fields for safety
    if (len < 5) {
        sequence = part = total = hops = flags = 0;
        return;
    }

    // Combine first two bytes into a 16-bit sequence number (big-endian)
    sequence = (in_buf[0] << 8) | in_buf[1];

    // Extract fragment index
    part     = in_buf[2];

    // Extract total number of parts
    total    = in_buf[3];

    // Extract hops from upper 4 bits of byte 4
    hops     = (in_buf[4] >> 4) & 0x0F;

    // Extract flags from lower 4 bits of byte 4
    flags    = in_buf[4] & 0x0F;
}


// Portable (Arduino-safe) version: no snprintf, only manual int-to-string.
void MessageID::to_string(char* out, size_t max_len) const {
    // Minimal, heap-free, Arduino/embedded safe: use simple integer to string conversion.
    // Example: "SEQ:1234 PART:2/3 HOPS:1 FLAGS:0xA"
    // Buffer should be at least 40 bytes; overflow is clipped.
    int idx = 0;
    #define PUT(s) do { const char* p = (s); while (*p && idx < int(max_len)-1) out[idx++] = *p++; } while (0)
    #define PUTHEX(x) do { \
        uint8_t v = (x); \
        if (v < 10) out[idx++] = '0' + v; \
        else if (v < 16) out[idx++] = 'A' + (v-10); \
    } while(0)
    // -- Compose
    PUT("SEQ:"); // Sequence number
    int val = sequence;
    char tmp[6]; int ti = 0;
    if (val == 0) tmp[ti++] = '0';
    else { int v = val, digits[5], d = 0; while (v && d < 5) { digits[d++] = v % 10; v /= 10; }
        while (d--) tmp[ti++] = '0' + digits[d]; }
    for (int j=0;j<ti&&idx<int(max_len)-1;++j) out[idx++] = tmp[j];
    PUT(" PART:");
    tmp[0] = '0' + (part / 10); tmp[1] = '0' + (part % 10); int plen = (part>=10)?2:1;
    for(int j=(part>=10?0:1);j<plen;++j) out[idx++] = tmp[j];
    out[idx++] = '/';
    tmp[0] = '0' + (total / 10); tmp[1] = '0' + (total % 10); int tlen = (total>=10)?2:1;
    for(int j=(total>=10?0:1);j<tlen;++j) out[idx++] = tmp[j];
    PUT(" HOPS:");
    out[idx++] = '0' + (hops / 10);
    out[idx++] = '0' + (hops % 10);
    PUT(" FLAGS:0x");
    uint8_t f = flags;
    if (f >= 10) out[idx++] = 'A' + (f - 10); else out[idx++] = '0' + f;
    out[idx] = 0; // null terminate
    #undef PUT
    #undef PUTHEX
}

// =============================================================================
// Getting & Setting
// =============================================================================

// Check if the "request acknowledgment" flag (bit 0) is set
bool MessageID::requests_acknowledgment() const { return (flags & 0x1) != 0; }

// Check if the "acknowledgment" flag (bit 1) is set
bool MessageID::is_acknowledgment() const { return (flags & 0x2) != 0; }

// Check if the "encrypted" flag (bit 2) is set
bool MessageID::is_encrypted() const { return (flags & 0x4) != 0; }


// Set or clear the "request acknowledgment" flag (bit 0)
void MessageID::set_request_acknowledgment(bool enable) { if (enable) flags |= 0x1; else flags &= ~0x1; }

// Set or clear the "acknowledgment" flag (bit 1)
void MessageID::set_is_acknowledgment(bool enable) { if (enable) flags |= 0x2; else flags &= ~0x2; }

/// Set or clear the "encrypted" flag (bit 2)
void MessageID::set_is_encrypted(bool enable) { if (enable) flags |= 0x4; else flags &= ~0x4; }

// =============================================================================
// Conversion
// =============================================================================

// Convert a single hexadecimal character (0–9, A–F, a–f) into its numeric value (0–15)
bool MessageID::hex_char_to_val(char c, uint8_t& out) {
    // If character is between '0' and '9', subtract '0' to get value (e.g., '3' → 3)
    if ('0' <= c && c <= '9') {
        out = c - '0';
        return true;
    }

    // If character is between 'A' and 'F', subtract 'A' and add 10 (e.g., 'B' → 11)
    if ('A' <= c && c <= 'F') {
        out = c - 'A' + 10;
        return true;
    }

    // If character is between 'a' and 'f', subtract 'a' and add 10 (e.g., 'd' → 13)
    if ('a' <= c && c <= 'f') {
        out = c - 'a' + 10;
        return true;
    }

    // Invalid hex character — return false and leave `out` unchanged
    return false;
}


// Convert a hexadecimal string (e.g., "4F2B000131") into a byte array
bool MessageID::hex_str_to_bytes(const char* str, uint8_t* out_bytes, size_t bytes_needed) {
    size_t len = 0;
    const char* p = str;

    // Skip optional "0x" or "0X" prefix if present
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }

    // Compute the length of the remaining string (up to 20 characters max)
    const char* q = p;
    while (q[len] && ++len < 20) {}

    // The hex string must be exactly 2 characters per byte (no more, no less)
    if (len != bytes_needed * 2) return false;

    // Parse each byte: two hex characters → one byte
    for (size_t i = 0; i < bytes_needed; ++i) {
        uint8_t hi = 0, lo = 0;

        // Convert high nibble (first hex digit of the pair)
        if (!hex_char_to_val(p[i * 2], hi)) return false;

        // Convert low nibble (second hex digit of the pair)
        if (!hex_char_to_val(p[i * 2 + 1], lo)) return false;

        // Combine high and low nibbles into a full byte
        out_bytes[i] = (hi << 4) | lo;
    }

    // Successfully converted all hex pairs into bytes
    return true;
}

} // namespace viatext
