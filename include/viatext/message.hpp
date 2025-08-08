/**
 * @file message.hpp
 * @brief ViaText Message Class — Universal Protocol Message Container for All Node Types
 *
 * ---
 *
 * ## Overview
 *
 * The `Message` class defines the standard container for all messages
 * within the ViaText protocol. It wraps routing metadata (`MessageID`),
 * source and destination identifiers (`from`, `to`), and the payload
 * (`data`) in a single unified structure that can be used across:
 *
 * - LoRa-based microcontroller nodes
 * - Linux CLI and Station programs
 * - Daemon and service components
 * - Sneakernet relays or gateways
 *
 * ---
 *
 * ## Purpose
 *
 * This class provides a **normalized interface for all message handling**.
 * Whether the message is human-authored or machine-generated, inbound or outbound,
 * fragmented or complete — this object abstracts away the low-level details and
 * presents a clean, zero-overhead interface for:
 *
 * - Core processing
 * - Validation
 * - Fragmentation logic
 * - Display and debugging
 * - Serialization to/from the wire format
 *
 * ---
 *
 * ## Microcontroller-Friendly by Design
 *
 * Like all core ViaText components, this class:
 * - ✅ Avoids all dynamic memory (heap-free)
 * - ✅ Uses ETL containers for `etl::string` and fixed-size fragments
 * - ✅ Avoids STL, exceptions, RTTI, or heavy constructs
 * - ✅ Runs identically on Linux and embedded targets (ESP32, ATmega, etc.)
 *
 * The `data` field uses a `TextFragments<8, 32>` container,
 * keeping total memory footprint around **256 bytes** for the payload
 * — just enough to carry:
 *
 * - The message header (sequence, hops, flags)
 * - The sender/recipient IDs (6 chars each)
 * - Actual user text or machine directives (`-rssi`, `-data`, etc.)
 *
 * ---
 *
 * ## Wire Format and Parsing
 *
 * Messages are often transmitted as a single delimited string:
 *
 * ```
 * <id>~<from>~<to>~<data>
 * ```
 *
 * Example:
 * ```
 * 0x4F2B000131~shrek~donkey~Shut Up
 * ```
 *
 * The `Message` class supports:
 * - `from_wire_string()` → parse raw string into structured fields
 * - `to_wire_string()`   → convert structured message to wire string
 *
 * This keeps routing and payloads compact and easy to debug.
 *
 * ---
 *
 * ## Error Codes (`error` field)
 *
 * | Code | Meaning                      |
 * |------|------------------------------|
 * | 0    | OK                           |
 * | 1    | Parse error (invalid input)  |
 * | 2    | Overflow (too large)         |
 * | 3    | Empty or uninitialized       |
 * | 4    | Fragmentation/parsing issue  |
 *
 * ---
 *
 * ## Fragmentation & State Awareness
 *
 * The class also provides:
 * - `is_fragmented()` → if this is part of a multi-part message
 * - `is_complete()`   → if all fragments have been received
 * - `is_valid()`      → for general well-formedness
 *
 * These help with intermediate routing and reassembly logic.
 *
 * ---
 *
 * ## Design Philosophy
 *
 * This class exists to **normalize communication** between all nodes, regardless of medium.
 * It encapsulates everything needed to:
 *
 * - Route messages across unreliable networks
 * - Store/forward messages in mesh-based relays
 * - Display messages clearly in terminals
 * - Avoid silent corruption or inconsistency
 *
 * ---
 *
 * ## Integration Points
 *
 * - Used directly by: `Core`, `CLI`, `LoRa wrappers`, and Station apps
 * - Built using:
 *   - `MessageID` for routing metadata
 *   - `TextFragments` for payload chunking
 * - Serializes to/from standardized wire string format
 *
 * ---
 *
 * ## Authors
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-07
 */
#ifndef VIATEXT_MESSAGE_HPP
#define VIATEXT_MESSAGE_HPP

#include "message_id.hpp"
#include "text_fragments.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

// These can be tuned for MCU limits.

/**
 * @brief Maximum character length for the sender field (`from`).
 * 
 * Supports short, radio-style IDs like `"NODE1"` or `"HCKRmn"`.
 * This value directly sets the capacity of the `etl::string` used in `from`.
 */
constexpr size_t FROM_LEN = 6;

/**
 * @brief Maximum character length for the recipient field (`to`).
 *
 * Follows the same constraints as `FROM_LEN` to ensure compact addressing.
 * Common values: `"TX1"`, `"BASE2"`, etc.
 */
constexpr size_t TO_LEN = 6;

/**
 * @brief Maximum number of text fragments the message payload (`data`) can contain.
 *
 * Each fragment is a fixed-capacity ETL string (`FRAG_SIZE` bytes).
 * The total payload space is `DATA_FRAGS × FRAG_SIZE` bytes.
 * Default is 8 × 32 = 256 bytes — LoRa/serial-friendly.
 */
constexpr size_t DATA_FRAGS = 8;

/**
 * @brief Maximum number of characters in a single payload fragment.
 *
 * All fragments in the message share this size limit.
 * Helps enforce consistency across Linux and microcontroller nodes.
 */
constexpr size_t FRAG_SIZE = 32;


/**
 * @class Message
 * @brief The universal protocol message container for the ViaText communication system.
 *
 * This class encapsulates a single ViaText message, including:
 * - Routing metadata (`MessageID`)
 * - Sender and recipient identifiers (`from`, `to`)
 * - A message body, broken into fixed-size text fragments (`data`)
 *
 * Messages can originate from human users, IoT devices, scripts, or mesh-relaying nodes.
 * Regardless of source or destination, every message in the system eventually
 * conforms to this object model.
 *
 * ---
 *
 * ## Structure
 * A `Message` consists of:
 * - `id`    → A compact 5-byte binary message header (sequence, hops, flags)
 * - `from`  → Short alphanumeric node ID (e.g. `"NODE1"`)
 * - `to`    → Intended recipient node ID
 * - `data`  → Payload in 8 × 32-byte fixed fragments (`TextFragments`)
 *
 * This results in a fully MCU-safe, 256-byte payload envelope with no dynamic memory.
 *
 * ---
 *
 * ## Parsing and Serialization
 *
 * Messages can be constructed from or converted to the **wire string format**:
 *
 * ```
 * <id>~<from>~<to>~<data>
 * ```
 *
 * Example:
 * ```
 * 0x4F2B000131~shrek~donkey~Shut Up
 * ```
 *
 * This enables compatibility across:
 * - LoRa and serial interfaces
 * - Linux CLI input/output
 * - File-based sneakernet transfers
 *
 * ---
 *
 * ## Embedded-Safe Design
 * The class uses only:
 * - `etl::string` (bounded, stack-safe)
 * - `TextFragments` (fragmented text buffer)
 * - No heap allocation
 * - No STL or dynamic containers
 *
 * This makes it safe to use in constrained environments like:
 * - ESP32 (LoRa)
 * - Arduino Mega / ATmega
 * - Linux daemons and shells
 *
 * ---
 *
 * ## Utility Methods
 * - `is_valid()`       → Checks parse success and structural integrity
 * - `is_fragmented()`  → Returns true if `id.total > 1`
 * - `is_complete()`    → True if all expected fragments are present
 * - `to_wire_string()` → Serialize into a single `~`-delimited output
 * - `from_wire_string()` → Populate from a `const char*` wire string
 * - `to_string()`      → Debug-friendly printable version
 *
 * ---
 *
 * ## Use Cases
 * - Message routing and forwarding
 * - User message display
 * - Command transport (`-set_id`, `-ack`, etc.)
 * - LoRa-to-Linux translation
 * - Mesh storage and persistence
 *
 * ---
 *
 * @note All fields are public for zero-cost access in tight loops and device code.
 * @note This class forms the canonical “message envelope” in the ViaText protocol.
 */

class Message {
public:
    // --- Fields (all public for zero-overhead struct-style use) ---

    /**
     * @brief The compact 5-byte routing and fragmentation header for this message.
     *
     * Includes sequence number, part number, total parts, hop count, and delivery flags.
     * Used to track message delivery, deduplication, and multi-part reassembly.
     */
    MessageID id;

    /**
     * @brief Sender node ID (max 6 characters).
     *
     * This identifies the origin of the message.
     * Format must match ViaText callsign rules: A–Z, 0–9, `_`, `-`.
     */
    etl::string<FROM_LEN> from;

    /**
     * @brief Recipient node ID (max 6 characters).
     *
     * This is the intended final destination of the message.
     * Nodes may still relay the message even if not addressed to them.
     */
    etl::string<TO_LEN> to;

    /**
     * @brief The message payload, split into fixed-size text fragments.
     *
     * Typically holds plain-text message body or machine arguments (e.g., `-rssi 92 -data hello`).
     * Each fragment is `FRAG_SIZE` bytes, with up to `DATA_FRAGS` total.
     * Fragmentation ensures compatibility with LoRa and serial limits.
     */
    TextFragments<DATA_FRAGS, FRAG_SIZE> data;


    /**
     * @brief Error state for this message instance.
     *
     * Used to detect issues during construction, parsing, or payload fragmentation.
     *
     * | Code | Meaning                          |
     * |------|----------------------------------|
     * | 0    | OK (no error)                    |
     * | 1    | Parse error (invalid wire format)|
     * | 2    | Overflow (data exceeds limits)   |
     * | 3    | Empty message or uninitialized   |
     * | 4    | Fragmentation or decode failure  |
     */
    uint8_t error = 0;


    // --- Constructors ---

    /**
     * @brief Default constructor.
     *
     * Creates an empty message with no fields set.
     * The `error` flag will be `3` (empty) until fields are assigned manually.
     */
    Message();

    /**
     * @brief Construct a message from raw components.
     *
     * Initializes a message using a known `MessageID`, sender (`from_`), recipient (`to_`),
     * and message body (`data_`).
     *
     * This is commonly used when constructing a message for transmission.
     *
     * @param id_    Pre-built MessageID struct.
     * @param from_  Sender node ID string.
     * @param to_    Recipient node ID string.
     * @param data_  Raw payload string to be split into fragments.
     */
    Message(const MessageID& id_, const char* from_, const char* to_, const char* data_);

    /**
     * @brief Construct a message by parsing a wire-formatted string.
     *
     * Expected input format: `<id>~<from>~<to>~<data>`.
     * If parsing fails, the `error` field will be set accordingly.
     *
     * @param wire_str Null-terminated string in ViaText wire format.
     */
    Message(const char* wire_str);


    // --- Field setters ---

    /**
     * @brief Set the message ID (sequence, part, hops, flags).
     * @param id_ A preconstructed MessageID struct.
     */
    void set_id(const MessageID& id_);

    /**
     * @brief Set the sender node ID.
     * @param from_ A null-terminated string (max 6 chars).
     */
    void set_from(const char* from_);

    /**
     * @brief Set the recipient node ID.
     * @param to_ A null-terminated string (max 6 chars).
     */
    void set_to(const char* to_);

    /**
     * @brief Set the message payload.
     *
     * The input string is split into fragments using `TextFragments<8, 32>`.
     * If the payload exceeds available space, the `error` field will be set.
     *
     * @param data_ A null-terminated message string (typically under 256 bytes).
     */
    void set_data(const char* data_);

    /**
     * @brief Reset all fields to their default (empty) state.
     *
     * Clears `id`, `from`, `to`, `data`, and resets the `error` code to 3 (empty).
     */
    void clear();


    // --- Field getters ---

    /**
     * @brief Get the message ID (routing header).
     * @return Reference to the `MessageID` struct.
     */
    const MessageID& get_id() const;

    /**
     * @brief Get the sender node ID.
     * @return Reference to the ETL string containing the `from` field.
     */
    const etl::string<FROM_LEN>& get_from() const;

    /**
     * @brief Get the recipient node ID.
     * @return Reference to the ETL string containing the `to` field.
     */
    const etl::string<TO_LEN>& get_to() const;

    /**
     * @brief Get the payload fragments.
     * @return Reference to the `TextFragments` container holding the message body.
     */
    const TextFragments<DATA_FRAGS, FRAG_SIZE>& get_data() const;


    // --- Serialization ---

    /**
     * @brief Convert this message into a wire-format string.
     *
     * Output format: `<id>~<from>~<to>~<data>`, where all fields are text.
     * The `data` field is serialized as a single string by joining fragments.
     *
     * @param out     Destination character buffer.
     * @param max_len Maximum number of bytes to write (including null terminator).
     */
    void to_wire_string(char* out, size_t max_len) const;

    /**
     * @brief Populate this message from a wire-format string.
     *
     * Input format must follow: `<id>~<from>~<to>~<data>`.
     * On success, returns `true` and sets all fields.  
     * On failure, sets `error` and returns `false`.
     *
     * @param wire_str Null-terminated input string.
     * @return `true` if parsing succeeded; `false` otherwise.
     */
    bool from_wire_string(const char* wire_str);


    // --- Validity, fragmentation, and flags ---

    /**
     * @brief Check if the message is structurally valid.
     *
     * Returns `true` if all required fields are present and `error == 0`.
     * A valid message has a populated ID, sender, recipient, and data fragments.
     *
     * @return `true` if the message is well-formed.
     */
    bool is_valid() const;

    /**
     * @brief Check if this message is part of a multi-part transmission.
     *
     * A message is considered fragmented if `id.total > 1`.
     * Used to determine whether additional fragments are expected.
     *
     * @return `true` if this is a fragmented message.
     */
    bool is_fragmented() const;

    /**
     * @brief Check if all fragments for this message have been received.
     *
     * Returns `true` if the current part index is less than total parts
     * and the `data` field appears complete.
     *
     * @return `true` if the message is complete and ready for processing.
     */
    bool is_complete() const;

    // --- Debugging/inspection ---

    /**
     * @brief Generate a human-readable representation of the message.
     *
     * Similar to `to_wire_string()`, but may include formatting or truncation
     * to aid with debugging, logs, or terminal display.
     *
     * @param out     Destination buffer for formatted string.
     * @param max_len Maximum size of the output buffer.
     */
    void to_string(char* out, size_t max_len) const;

};

} // namespace viatext

#endif // VIATEXT_MESSAGE_HPP
