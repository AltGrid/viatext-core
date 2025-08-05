/**
 * @file message_id.hpp
 * @brief ViaText MessageID struct — Compact 5-byte message header for LoRa & mesh networks.
 * 
 * This header defines the MessageID struct, which is used in the ViaText core to encode and decode the 
 * compact, binary message identifiers required for the ViaText protocol.
 * 
 * @section via_message_id_intro ViaText Message ID System
 * 
 * In ViaText, every message that is sent over LoRa (or other transports) starts with a packed 5-byte 
 * "message ID" header. This header contains all the essential information for routing, fragmentation, 
 * and delivery control in a mesh network:
 * 
 * - A unique sequence number (16 bits) that identifies the whole message
 * - A part number and total parts (8 bits each) to support splitting ("fragmenting") large messages
 * - A hop counter (4 bits) that limits rebroadcasting (TTL-style)
 * - Four flag bits (4 bits) for delivery state (like ACK or encryption)
 * 
 * This struct allows the message header to be easily built and parsed, making it possible for any 
 * device (Linux, ESP32, etc.) to participate in the network using minimal memory and maximum portability.
 * 
 * @author Leo
 * @author ChatGPT
 */

#ifndef VIATEXT_MESSAGE_ID_HPP
#define VIATEXT_MESSAGE_ID_HPP

#include <stdint.h>
#include <stddef.h>

namespace viatext {

/**
 * @struct MessageID
 * @brief Represents a ViaText message identifier header, packed into 5 bytes for efficient transmission.
 *
 * This struct handles encoding and decoding the ViaText message ID header, which fits into just 5 bytes 
 * and is used in every message for addressing, fragmentation, routing, and state flags.
 * 
 * The 5-byte layout is as follows (bitwise):
 * 
 * | Field        | Bits | Size (Bytes) | Description                          |
 * |--------------|------|--------------|--------------------------------------|
 * | Sequence     | 16   | 2            | Unique message identifier            |
 * | Part Number  | 8    | 1            | Index of this part (0-based)         |
 * | Total Parts  | 8    | 1            | Total number of parts in the message |
 * | Hops         | 4    | — (within 1) | Hop count or TTL (max 15 hops)       |
 * | Flags        | 4    | — (within 1) | Acknowledge and Encryption flags     |
 *
 * Example: [0x00, 0x1C, 0x05, 0x07, 0xAC] would be: Sequence=28, Part=5, Total=7, Hops=10, Flags=0xC.
 * 
 * @note Designed for beginner-level readability and cross-platform (Linux, Arduino, ESP32, etc.).
 */
struct MessageID {
    /**
     * @brief Sequence number for this message (16 bits, 0–65535).
     * 
     * This uniquely identifies a whole message, even if it is sent in fragments.
     * For example, message 42 with 3 parts will use the same sequence=42 for all 3.
     */
    uint16_t sequence;

    /**
     * @brief The part number of this fragment (8 bits, 0–255).
     * 
     * If a message is split into multiple parts, this number tells which fragment this is.
     * 0 = first part, 1 = second part, etc.
     */
    uint8_t part;

    /**
     * @brief The total number of parts in the full message (8 bits, 0–255).
     * 
     * If the message is not split, total will be 1.
     */
    uint8_t total;

    /**
     * @brief Hop count or TTL (Time To Live) for mesh routing (4 bits, 0–15).
     * 
     * This is used to prevent a message from endlessly circulating in the mesh. 
     * Each time a message is rebroadcast, hops increases. If hops >= TTL, the message is dropped.
     * 
     * @note Only the upper 4 bits of the 5th byte are used for hops.
     */
    uint8_t hops;

    /**
     * @brief Flags for message delivery state and encryption (4 bits, 0–15).
     *
     * Each bit in this 4-bit field has a specific meaning for routing and reliability:
     * - Bit 0 (0x1): request_acknowledge — Set if the sender requests an ACK from the recipient.
     * - Bit 1 (0x2): acknowledgment     — Set if this message is an acknowledgment (ACK) reply.
     * - Bit 2 (0x4): is_encrypted       — Set if the payload is encrypted.
     * - Bit 3 (0x8): unused             — Reserved for future use; must be zero.
     *
     * These bits allow efficient signaling of message state and behavior in every ViaText message.
     *
     * @note Only the lower 4 bits of the 5th header byte are used for flags.
     * @note You can set/clear these bits directly or use bitwise operators to test them:
     *   Example: if (flags & 0x2)  // this is an ACK
     */
    uint8_t flags;


    // --- Constructors ---

    /**
     * @brief Default constructor.
     * 
     * Sets all fields (sequence, part, total, hops, flags) to zero.
     */
    MessageID();

    /**
     * @brief Constructs a MessageID from a single 40-bit (5-byte) integer value.
     *
     * This constructor allows you to create a MessageID from an undivided 5-byte hexadecimal value.
     * The value should be packed in **big-endian** (network order), meaning the most significant
     * byte comes first. For example, if your message ID is 0x4F2B000131, it will be unpacked
     * as bytes [0x4F, 0x2B, 0x00, 0x01, 0x31].
     *
     * This is convenient if you already have the message header as a single integer,
     * such as when receiving or storing it as a fixed-size value in memory.
     *
     * @param five_byte_value The 5-byte (40-bit) message ID, given as a `uint64_t`.
     *
     * @note Only the least significant 40 bits are used. Higher bits are ignored.
     * @note If your value is less than 5 bytes, it will be zero-padded on the left.
     *
     * **Usage Example:**
     * @code
     * viatext::MessageID msgid(0x4F2B000131ULL);
     * @endcode
     */
    MessageID(uint64_t five_byte_value);

    /**
     * @brief Constructs a MessageID by parsing a hex string.
     *
     * This constructor allows you to create a MessageID from a string containing 5 bytes in
     * hexadecimal format. The string can start with "0x" or not, and should be exactly
     * 10 hex digits long (e.g., "0x4F2B000131" or "4F2B000131").
     *
     * Each pair of hex digits is interpreted as a single byte, in **big-endian** order.
     * For example, "0x4F2B000131" is converted to bytes [0x4F, 0x2B, 0x00, 0x01, 0x31].
     *
     * If the input is not a valid 10-digit hex string, all fields are set to zero.
     *
     * @param hex_str The hex string representing the 5-byte message ID.
     *
     * @note This does not accept spaces, colons, or other separators.
     * @note Both upper and lower case hex letters are accepted.
     *
     * **Usage Example:**
     * @code
     * viatext::MessageID msgid("0x4F2B000131");
     * viatext::MessageID msgid2("4F2B000131");
     * @endcode
     */
    MessageID(const char* hex_str);

    
    /**
     * @brief Constructs MessageID from a 5-byte packed array (usually from a received message).
     * 
     * @param data Pointer to a 5 or 5-byte array containing the message ID header.
     * @param len  Length of the input array. Only the first 5 bytes are required.
     * 
     * The data buffer should be formatted as:
     * [0]=sequence high, [1]=sequence low, [2]=part, [3]=total, [4]=hops/flags
     */
    MessageID(const uint8_t* data, size_t len);

    /**
     * @brief Constructs MessageID from explicit values for each field.
     * 
     * @param sequence Unique message sequence number (16-bit)
     * @param part     Fragment number (8-bit)
     * @param total    Number of fragments (8-bit)
     * @param hops     Hop counter/TTL (only lower 4 bits are used)
     * @param flags    Flags (only lower 4 bits are used)
     */
    MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops, uint8_t flags);

    /**
     * @brief Constructs MessageID with optional flags for ACK and encryption.
     * 
     * @param sequence Unique message sequence number (16-bit)
     * @param part     Fragment number (8-bit)
     * @param total    Number of fragments (8-bit)
     * @param request_acknowledge If true, sets the request_acknowledge flag (bit 0).
     * @param is_acknowledgment If true, sets the acknowledgment flag (bit 1).
     * @param is_encrypted If true, sets the is_encrypted flag (bit 2).
     */
    MessageID(uint16_t sequence, uint8_t part, uint8_t total, uint8_t hops,
              bool request_acknowledge = false, bool is_acknowledgment = false,
              bool is_encrypted = false, bool unused = false);
    /**
     * @brief Packs the fields into a 5-byte array for transmission.
     * 
     * Writes this object's fields to the given array in the correct ViaText binary format, 
     * suitable for LoRa or serial transmission.
     * 
     * @param out_buf Pointer to an array of at least 5 bytes, where packed data will be written.
     * 
     * Usage example:
     * @code
     * uint8_t buf[5];
     * msgid.pack(buf);
     * // buf now contains the 5-byte header
     * @endcode
     */
    void pack(uint8_t* out_buf) const;

    /**
     * @brief Populates fields by parsing a 5-byte (or 5-byte) array.
     * 
     * Reads the given packed array and extracts all fields (sequence, part, total, hops, flags).
     * 
     * @param in_buf Pointer to an array with packed data.
     * @param len    Length of the buffer (should be at least 5).
     * 
     * If the buffer is too short, all fields will be set to zero.
     * 
     * Usage example:
     * @code
     * uint8_t buf[5] = { ... };
     * MessageID msgid;
     * msgid.unpack(buf, 5);
     * @endcode
     */
    void unpack(const uint8_t* in_buf, size_t len);

    /**
     * @brief Writes a human-readable summary of the MessageID fields to a string.
     * 
     * Formats the fields as a single line of text, useful for logging or debugging.
     * 
     * Example output: "SEQ:28 PART:5/7 HOPS:10 FLAGS:0xC"
     * 
     * @param out      Pointer to the destination character buffer.
     * @param max_len  Maximum number of characters to write (including null terminator).
     * 
     * Usage example:
     * @code
     * char logbuf[64];
     * msgid.to_string(logbuf, sizeof(logbuf));
     * // logbuf contains a readable summary of the header
     * @endcode
     */
    void to_string(char* out, size_t max_len) const;
    
    /**
     * @brief Returns true if this message requests an acknowledgment from the recipient.
     *
     * This checks if the request_acknowledge flag (bit 0) is set in the flags field.
     *
     * @return true if the message is requesting an ACK, false otherwise.
     *
     * Usage:
     * @code
     * if (msgid.requests_acknowledgment()) { ... }
     * @endcode
     */
    bool requests_acknowledgment() const;

    /**
     * @brief Returns true if this message is an acknowledgment (ACK) reply.
     *
     * This checks if the acknowledgment flag (bit 1) is set in the flags field.
     *
     * @return true if this message is an ACK reply, false otherwise.
     *
     * Usage:
     * @code
     * if (msgid.is_acknowledgment()) { ... }
     * @endcode
     */
    bool is_acknowledgment() const;

    /**
     * @brief Returns true if this message payload is encrypted.
     *
     * This checks if the is_encrypted flag (bit 2) is set in the flags field.
     *
     * @return true if the payload is encrypted, false otherwise.
     *
     * Usage:
     * @code
     * if (msgid.is_encrypted()) { ... }
     * @endcode
     */
    bool is_encrypted() const;

    /**
     * @brief Sets or clears the "request acknowledgment" flag (bit 0).
     *
     * Use this to indicate whether this message should request an acknowledgment (ACK)
     * from the recipient. When set to true, the sender is requesting an ACK reply.
     *
     * @param enable Pass true to set the flag, false to clear it.
     *
     * Usage:
     * @code
     * msgid.set_request_acknowledgment(true);  // Ask for ACK
     * msgid.set_request_acknowledgment(false); // No ACK needed
     * @endcode
     */
    void set_request_acknowledgment(bool enable);

    /**
     * @brief Sets or clears the "acknowledgment" flag (bit 1).
     *
     * Use this to mark the message as an ACK reply (acknowledgment). When set to true,
     * this message will be recognized as an acknowledgment message.
     *
     * @param enable Pass true to set the flag, false to clear it.
     *
     * Usage:
     * @code
     * msgid.set_acknowledgment(true);  // This message is an ACK
     * msgid.set_acknowledgment(false); // This message is not an ACK
     * @endcode
     */
    void set_is_acknowledgment(bool enable);

    /**
     * @brief Sets or clears the "encrypted" flag (bit 2).
     *
     * Use this to indicate whether the message payload is encrypted.
     *
     * @param enable Pass true to set the flag, false to clear it.
     *
     * Usage:
     * @code
     * msgid.set_encrypted(true);  // Payload is encrypted
     * msgid.set_encrypted(false); // Payload is not encrypted
     * @endcode
     */
    void set_is_encrypted(bool enable);



private:
    /**
     * @brief Converts a single hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f') to its numeric value (0-15).
     *
     * This function takes a single character and attempts to convert it to a number from 0 to 15.
     * For example, 'A' or 'a' becomes 10, 'F' or 'f' becomes 15, '0' becomes 0, and so on.
     *
     * @param c The character to convert.
     * @param out Reference to a uint8_t where the result will be stored if conversion succeeds.
     * @return true if the character is a valid hex digit, false otherwise.
     *
     * **Usage Example:**
     * @code
     * uint8_t val;
     * if (MessageID::hex_char_to_val('B', val)) {
     *     // val == 11
     * }
     * @endcode
     */
    static bool hex_char_to_val(char c, uint8_t& out);

    /**
     * @brief Converts a hexadecimal string to a byte array.
     *
     * This function parses a hex string (e.g., "4F2B000131" or "0x4F2B000131") into a byte buffer.
     * The string must be exactly `bytes_needed * 2` hex digits (10 for 5 bytes), and may have an
     * optional "0x" or "0X" prefix. All hex digits must be valid ('0'-'9', 'A'-'F', 'a'-'f').
     *
     * @param str        The input string (with or without "0x" prefix).
     * @param out_bytes  The output byte buffer (should have at least bytes_needed size).
     * @param bytes_needed The number of bytes to parse from the string.
     * @return true if conversion is successful and the input is well-formed; false otherwise.
     *
     * **Usage Example:**
     * @code
     * uint8_t buffer[5];
     * if (MessageID::hex_str_to_bytes("0x4F2B000131", buffer, 5)) {
     *     // buffer now contains 0x4F, 0x2B, 0x00, 0x01, 0x31
     * }
     * @endcode
     */
    static bool hex_str_to_bytes(const char* str, uint8_t* out_bytes, size_t bytes_needed);
};

}
#endif // VIATEXT_MESSAGE_ID_HPP
