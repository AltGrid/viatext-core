/**
 * @page vt-core_message_id ViaText MessageID
 * @file message_id.hpp
 * @brief ViaText MessageID — Compact 5-byte routing header for resilient mesh delivery.
 *
 * This header defines the `MessageID` struct, which encodes the standardized 5-byte message header
 * used in the ViaText protocol. Each message starts with this compact header, allowing for efficient
 * routing, fragmentation, and delivery control over constrained transports like LoRa, serial links,
 * and sneakernet storage.
 * 
 * @section viatext_id_concept Message ID Overview
 * 
 * The ViaText protocol uses a fixed-length, binary message header composed of the following fields:
 * 
 * | Field        | Bits | Bytes | Description                              |
 * |--------------|------|-------|------------------------------------------|
 * | Sequence     | 16   | 2     | Message-wide unique ID                   |
 * | Part Number  | 8    | 1     | Which fragment this is (0–255)           |
 * | Total Parts  | 8    | 1     | Total number of fragments (1–255)        |
 * | Hops         | 4    | —     | Hop count for TTL-style rebroadcast      |
 * | Flags        | 4    | —     | Delivery status, ACK, encryption, etc.   |
 * 
 * These are packed into 5 bytes total:
 * - **Bytes 0–1**: 16-bit sequence (big endian)
 * - **Byte 2**: Part number
 * - **Byte 3**: Total parts
 * - **Byte 4**: Packed 4-bit hops (upper) + 4-bit flags (lower)
 * 
 * This structure allows each ViaText message to carry:
 * - A unique identity across fragmented parts
 * - Lightweight retry and duplicate detection
 * - Directional routing control (via `hops`)
 * - Bit-level delivery flags for reliability and encryption
 * 
 * ### Example
 * A message header like: `[0x00, 0x1C, 0x05, 0x07, 0xAC]` would represent:
 * - `Sequence = 28` (0x001C)
 * - `Part = 5`
 * - `Total = 7`
 * - `Hops = 10` (0xA)
 * - `Flags = 0xC` (ACK + Encrypted)
 * 
 * @note This struct is designed for beginner C++ users and embedded system compatibility.
 *       It is used on Linux, ESP32 (Arduino), and other cross-platform systems.
 * 
 * @note The final design aligns with the ViaText binary protocol: a minimalist,
 *       5-byte header that balances readability, transport neutrality, and robust routing.
 * 
 * @see vt-core BLUEPRINT.md for spec-level description.
 * 
 * @author Leo
 * @author ChatGPT
 */

#ifndef VIATEXT_MESSAGE_ID_HPP
#define VIATEXT_MESSAGE_ID_HPP

#include "etl/string.h"
#include <stdint.h>
#include <stddef.h>


namespace viatext {

/**
 * @struct MessageID
 * @brief Represents the ViaText message header, packed into 5 bytes for compact routing and control.
 *
 * The `MessageID` struct defines the fixed-format header used in every ViaText message.
 * This 5-byte structure allows devices to handle:
 * - **Unique identification** of a message via a 16-bit sequence number
 * - **Fragmentation support** via `part` and `total` fields
 * - **TTL-style rebroadcast control** with a 4-bit hop count
 * - **Delivery state signaling** using 4 bits of flags
 *
 * ### Byte layout (5 bytes total):
 *
 * | Byte | Contents         | Bits  | Description                                 |
 * |------|------------------|-------|---------------------------------------------|
 * | 0–1  | `sequence`       | 16    | Unique ID for the whole message             |
 * | 2    | `part`           | 8     | Index of this fragment (0 = first)          |
 * | 3    | `total`          | 8     | Number of total fragments in the message    |
 * | 4    | `hops:4 + flags:4` | 8  | Hops (upper 4 bits) and flags (lower 4 bits)|
 *
 * ### Use cases:
 * - Track individual message fragments across hops
 * - Detect duplicates or replays
 * - Signal ACKs, requests for ACK, encryption
 *
 * This struct is optimized for constrained networks like LoRa and serial links, and can be
 * easily parsed by microcontrollers and Linux machines alike.
 *
 * @note Only 5 bytes are transmitted per message ID — memory-efficient by design.
 * @note Flags and hop count share a single byte (split into high/low 4 bits).
 *
 * @author Leo
 * @author ChatGPT
 */
struct MessageID {
    /**
     * @brief Unique 16-bit ID shared by all fragments of a single message.
     *
     * This number identifies the overall message. If a message is split into multiple parts,
     * all fragments will share the same sequence number. It allows receiving nodes to group
     * parts correctly and detect duplicates.
     */
    uint16_t sequence;

    /**
     * @brief Fragment index within a message (0-based).
     *
     * This indicates which piece of the message this is.
     * For example, if total=3, then part=0,1,2 for each respective piece.
     * Used during reassembly.
     */
    uint8_t part;

    /**
     * @brief Total number of fragments this message contains.
     *
     * If the message is not fragmented, this will be 1.
     * Otherwise, receiving nodes use this to know how many parts to expect.
     */
    uint8_t total;

    /**
     * @brief Number of hops this message has taken across the mesh (0–15).
     *
     * This acts like a TTL (time to live). Each time the message is forwarded,
     * this value is incremented. Messages that exceed a predefined limit are dropped.
     *
     * @note Only the upper 4 bits of the final byte are used for this.
     */
    uint8_t hops;

    /**
     * @brief Delivery flags encoded in 4 bits (lower half of final byte).
     *
     * Bitmask used to indicate special message behavior or state:
     * - Bit 0 (0x1): Request acknowledgment (sender wants ACK)
     * - Bit 1 (0x2): Acknowledgment (this is an ACK)
     * - Bit 2 (0x4): Encrypted payload
     * - Bit 3 (0x8): Reserved for future use
     *
     * These flags help implement reliable delivery and basic privacy signaling.
     *
     * @note Only the lower 4 bits of the final byte are used for this.
     */
    uint8_t flags;


    /**
     * @brief Default constructor — initializes all fields to zero.
     */
    MessageID();

    /**
     * @brief Constructs a MessageID from a 5-byte integer (lowest 40 bits used).
     * @param five_byte_value A packed 5-byte value in big-endian order.
     */
    MessageID(uint64_t five_byte_value);

    /**
     * @brief Constructs a MessageID from a 10-digit hex string (e.g., "4F2B000131").
     * @param hex_str A null-terminated string, with or without "0x" prefix.
     */
    MessageID(const char* hex_str);

    /**
     * @brief Constructs a MessageID from a 5-byte buffer.
     * @param data Pointer to input buffer.
     * @param len Length of the buffer; must be at least 5.
     */
    MessageID(const uint8_t* data, size_t len);

    /**
     * @brief Constructs a MessageID from raw field values.
     */
    MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops, uint8_t flags);

    /**
     * @brief Constructs a MessageID with flag options (ack, encrypted, etc.).
     */
    MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops,
              bool request_acknowledge, bool is_acknowledgment,
              bool is_encrypted, bool unused);


    /**
     * @brief Packs the MessageID fields into a 5-byte buffer.
     * @param out_buf Pointer to an output buffer of at least 5 bytes.
     */
    void pack(uint8_t* out_buf) const;

    /**
     * @brief Unpacks a 5-byte buffer into the MessageID fields.
     * @param in_buf Pointer to input buffer.
     * @param len Length of the buffer; must be at least 5.
     */
    void unpack(const uint8_t* in_buf, size_t len);


    /**
     * @brief Converts the MessageID fields into a human-readable string. For debugging.
     * @param out Pointer to the output character buffer.
     * @param max_len Maximum number of characters to write, including null terminator.
     *
     * Format example: "SEQ:28 PART:5/7 HOPS:10 FLAGS:0xC"
     */
    void to_string(char* out, size_t max_len) const;


    /**
     * @brief Checks if the "request acknowledgment" flag is set.
     * @return true if an ACK is requested.
     */
    bool requests_acknowledgment() const;

    /**
     * @brief Checks if this message is an acknowledgment (ACK).
     * @return true if this is an ACK message.
     */
    bool is_acknowledgment() const;

    /**
     * @brief Checks if the message payload is marked as encrypted.
     * @return true if encrypted.
     */
    bool is_encrypted() const;

    /**
     * @brief Sets or clears the "request acknowledgment" flag.
     * @param enable Pass true to request an ACK; false to disable.
     */
    void set_request_acknowledgment(bool enable);

    /**
     * @brief Sets or clears the "acknowledgment" (ACK) flag.
     * @param enable Pass true if this message is an ACK.
     */
    void set_is_acknowledgment(bool enable);

    /**
     * @brief Sets or clears the "encrypted" flag.
     * @param enable Pass true to mark the payload as encrypted.
     */
    void set_is_encrypted(bool enable);


    etl::string<11> to_hex_string() const {
        uint8_t buf[5];
        pack(buf);
        etl::string<11> hex;

        for (int i = 0; i < 5; ++i) {
            char hi = "0123456789ABCDEF"[buf[i] >> 4];
            char lo = "0123456789ABCDEF"[buf[i] & 0x0F];
            hex += hi;
            hex += lo;
        }

        return hex;
    }

    

private:
    /**
     * @brief Converts a single hex character to its numeric value (0–15).
     * @param c The character to convert (e.g., 'A', 'f', '9').
     * @param out Reference to a byte where the result will be stored.
     * @return true if the input was a valid hex character.
     */
    static bool hex_char_to_val(char c, uint8_t& out);

    /**
     * @brief Converts a hex string to a byte array.
     * @param str Input string (e.g., "4F2B000131" or "0x4F2B000131").
     * @param out_bytes Output buffer to hold parsed bytes.
     * @param bytes_needed Number of bytes to parse (should match buffer size).
     * @return true if the string was valid and converted successfully.
     */
    static bool hex_str_to_bytes(const char* str, uint8_t* out_bytes, size_t bytes_needed);

};

} // namespace viatext

#endif // VIATEXT_MESSAGE_ID_HPP
