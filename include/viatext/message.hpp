/**
 * @file message.hpp
 * @brief ViaText Message Class — Universal Protocol Message Container for All Node Types
 * @details
 *    The Message class in ViaText represents the fundamental unit of communication
 *    between any two nodes—human, machine, or IoT device—across the entire ViaText mesh.
 *
 *    **Purpose:**
 *    --------------------------------------------------
 *    The Message class is designed to:
 *      - Store, access, and manipulate all standard ViaText protocol messages.
 *      - Provide a unified container for message header data (routing, fragmentation, state flags),
 *        sender/recipient identity, and the primary message data itself.
 *      - Support serialization to and from the official "on-the-wire" string format used in LoRa,
 *        serial, and all ViaText transports (e.g., `"4F2B000131~ALICE~BOB~Hello Bob"`).
 *      - Offer simple, safe, beginner-friendly methods for every field—so new developers, AI bots,
 *        and embedded systems can reliably create, read, and update messages.
 *
 *    **Message Format:**
 *    --------------------------------------------------
 *    Every message in ViaText consists of:
 *      - A compact, 5-byte binary header (the MessageID struct) that encodes sequence, fragment info,
 *        hops (for mesh routing), and status flags (ACK, encryption, etc).
 *      - The sender's callsign ("from"), a short, human-readable ID (A–Z, 0–9, underscore, or hyphen).
 *      - The recipient's callsign ("to"), using the same format as "from".
 *      - The main message data ("data")—the actual text or payload being transmitted.
 *
 *    **On-the-Wire String Format:**
 *      - All ViaText messages are serialized as a tilde-delimited string:
 *        `[HEADER_HEX]~[FROM]~[TO]~[DATA]`
 *        Example: `"4F2B000131~SHREK~DONKEY~Shut Up"`
 *      - This format is 100% portable between Linux, ESP32/Arduino.
 *
 *    **Design Principles:**
 *    --------------------------------------------------
 *      - **Simplicity:** Minimal, easy-to-understand C++ for beginners and cross-platform builds.
 *      - **Safety:** All fields are protected, never raw pointers, and fully accessible via getters/setters.
 *      - **Compatibility:** Only uses standard C++ containers and safe STL features—no complex templates,
 *        no dynamic memory beyond std::string, and works on embedded (ESP32) as well as Linux.
 *      - **Extensibility:** Additional fields can be added in the future without breaking compatibility.
 *      - **Human & Machine Friendly:** Methods are clearly named; code and comments are optimized for
 *        both people and code-searching AI systems (IDE tooltips, Doxygen, Copilot, Google, etc).
 *
 *    **Typical Use Cases:**
 *    --------------------------------------------------
 *      - Assembling a message to send from a Linux CLI node to a remote LoRa radio.
 *      - Receiving, decoding, and logging messages on an ESP32 LilyGo LoRa device.
 *      - Relaying and rebroadcasting messages in a ViaText mesh network.
 *      - Inspecting message contents for debug, testing, or visualization.
 *
 *    For full protocol and field documentation, see the main ViaText documentation.
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-05
 */

#pragma once
#include <string>
#include <vector>
#include "message_id.hpp"

namespace viatext {

/**
 * @class Message
 * @brief The universal protocol message container for the ViaText communication system.
 * @details
 *    The Message class represents a **single ViaText message**—the atomic unit of communication
 *    exchanged by all nodes (human users, IoT devices, relays, and servers) in the ViaText network.
 *
 *    **What Does This Class Do?**
 *    ----------------------------------------------------------------------------
 *      - Encapsulates all core fields of a ViaText protocol message, including:
 *          - The binary header (MessageID struct) containing routing, fragmentation, hops,
 *            and status flags (ACK, encryption, etc).
 *          - The sender ID ("from") and recipient ID ("to"), using human-readable callsigns.
 *          - The main data/payload string ("data"), which can contain any user message or content.
 *      - Provides safe, simple **getters and setters for every field**, making message manipulation
 *        easy and reliable—even for C++ beginners or embedded programmers.
 *      - Enables **serialization and deserialization** to/from the ViaText wire protocol format
 *        (a single tilde-delimited string, e.g. "4F2B000131~ALICE~BOB~Hello Bob").
 *      - Supports reading and writing the binary header for efficient transmission over LoRa,
 *        serial, or other constrained networks.
 *      - Offers helper methods to check validity, completeness, fragmentation, and state flags.
 *      - Allows rapid message creation from scratch, from received data, or from a parsed string.
 *
 *    **How to Use This Class:**
 *    ----------------------------------------------------------------------------
 *      - **To build a new message:**
 *          - Create a Message object, set each field using setters, or use a constructor.
 *          - Serialize to a wire string using `to_wire_string()` to send over the network.
 *      - **To decode a received message:**
 *          - Use the string constructor or `from_wire_string()` to parse the tilde-delimited string.
 *          - Access fields using getters, and inspect message state (fragmented, complete, etc).
 *      - **For routing, deduplication, and handling fragments:**
 *          - Use the embedded MessageID struct and corresponding getters/setters for all header fields.
 *      - **For logging and debugging:**
 *          - Call `to_string()` to get a readable version for human or AI review.
 *
 *    **Protocol Compliance:**
 *    ----------------------------------------------------------------------------
 *      - All message field names, formats, and limits are strictly aligned with the ViaText protocol.
 *      - The Message class is designed to work identically on Linux and ESP32/Arduino.
 *      - Fields and methods are carefully named for clarity and discoverability by IDEs, Doxygen,
 *        and AI-based code tools.
 *
 *    **Why Does This Matter?**
 *    ----------------------------------------------------------------------------
 *      - The Message class is the **bridge between raw network data and meaningful communication**.
 *      - It abstracts away the complexity of headers, parsing, and packing, so even novice developers
 *        can work with advanced mesh, radio, or IoT systems without learning low-level binary protocols.
 *      - By making messages human- and machine-friendly, it fosters learning, debugging, automation,
 *        and future extension—whether for professional deployment or hobbyist tinkering.
 *
 *    **Typical Example:**
 *    ----------------------------------------------------------------------------
 *    @code
 *      viatext::Message msg;
 *      msg.set_sequence(1234);
 *      msg.set_from("NODE1");
 *      msg.set_to("NODE2");
 *      msg.set_data("Temperature is 22C");
 *      std::string wire = msg.to_wire_string(); // Ready to transmit!
 *    @endcode
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-05
 */
class Message {
public:
    /**
     * @brief Default constructor.
     * @details
     *   Initializes an empty ViaText message with all fields set to zero or blank.
     *   Use this to create a new message that you will fill out using setters.
     */
    Message();

    /**
     * @brief Full-field constructor.
     * @param id    The MessageID header struct (routing, fragmentation, flags, etc).
     * @param from  The sender's callsign (short string, e.g. "ALICE").
     * @param to    The recipient's callsign (short string, e.g. "BOB").
     * @param data  The message body or main data payload (arbitrary string).
     * @details
     *   Use this constructor to create a complete message in one step.
     *   All fields are copied from the arguments provided.
     */
    Message(const MessageID& id, const std::string& from, const std::string& to, const std::string& data);

    /**
     * @brief Construct a Message from a wire-format string.
     * @param wire_str The tilde-delimited protocol string (e.g., "4F2B000131~ALICE~BOB~Hello!").
     * @details
     *   Parses the given wire-format string and fills all fields accordingly.
     *   Intended for received messages or quick construction from the protocol format.
     */
    Message(const std::string& wire_str);

    /**
     * @brief Construct a Message from a packed binary buffer (header only).
     * @param buf  Pointer to a buffer containing at least 5 bytes (the packed MessageID).
     * @param len  Length of the buffer in bytes.
     * @details
     *   Loads the message header fields from a received or stored byte array.
     *   Only the MessageID is set; from/to/data remain empty.
     */
    Message(const uint8_t* buf, size_t len);

    /**
     * @brief Clears all fields in the message, resetting to the default/empty state.
     * @details
     *   Sets header fields to zero, and empties from, to, and data fields.
     *   Useful for object reuse or before parsing a new message.
     */
    void clear();

    /**
     * @brief Get a constant reference to the MessageID header struct.
     * @return The internal MessageID (for routing, deduplication, flags, etc).
     */
    const MessageID& get_id() const;

    /**
     * @brief Set the MessageID header struct for this message.
     * @param id The MessageID to copy in.
     */
    void set_id(const MessageID& id);

    /**
     * @brief Get the message sequence number (from header).
     * @return 16-bit sequence value, uniquely identifying the overall message (not just fragment).
     */
    uint16_t get_sequence() const;

    /**
     * @brief Set the message sequence number (header field).
     * @param seq The new sequence value to assign (0–65535).
     */
    void set_sequence(uint16_t seq);

    /**
     * @brief Get the fragment part number (from header).
     * @return 8-bit part number (0 = first part, 1 = second, etc).
     */
    uint8_t get_part() const;

    /**
     * @brief Set the fragment part number.
     * @param part The fragment index (0–255).
     */
    void set_part(uint8_t part);

    /**
     * @brief Get the total number of fragments in this message (from header).
     * @return 8-bit total part count (1 = not fragmented).
     */
    uint8_t get_total() const;

    /**
     * @brief Set the total number of fragments in this message.
     * @param total The total part count (1–255).
     */
    void set_total(uint8_t total);

    /**
     * @brief Get the hop count (TTL/mesh rebroadcast limit) from the header.
     * @return 4-bit hop value (0–15).
     */
    uint8_t get_hops() const;

    /**
     * @brief Set the hop count (TTL/mesh rebroadcast limit).
     * @param hops The hop value to assign (only lower 4 bits used).
     */
    void set_hops(uint8_t hops);

    /**
     * @brief Get the raw 4-bit flags field from the header (ACK, encryption, etc).
     * @return Flags as a single byte (lower 4 bits meaningful).
     */
    uint8_t get_flags() const;

    /**
     * @brief Set the raw 4-bit flags field in the header.
     * @param flags The new flags value (only lower 4 bits used).
     */
    void set_flags(uint8_t flags);

    /**
     * @brief Check if this message requests an acknowledgment reply (from flags).
     * @return True if the "request acknowledgment" bit is set.
     */
    bool requests_acknowledgment() const;

    /**
     * @brief Set or clear the "request acknowledgment" bit in the header.
     * @param enable True to request ACK from the recipient, false to clear.
     */
    void set_request_acknowledgment(bool enable);

    /**
     * @brief Check if this message is itself an acknowledgment reply (from flags).
     * @return True if the "acknowledgment" bit is set.
     */
    bool is_acknowledgment() const;

    /**
     * @brief Set or clear the "acknowledgment" bit (header).
     * @param enable True to mark as ACK, false otherwise.
     */
    void set_is_acknowledgment(bool enable);

    /**
     * @brief Check if this message's data field is encrypted (from flags).
     * @return True if the "encrypted" bit is set.
     */
    bool is_encrypted() const;

    /**
     * @brief Set or clear the "encrypted" bit in the header.
     * @param enable True to mark as encrypted, false otherwise.
     */
    void set_is_encrypted(bool enable);

    /**
     * @brief Get the sender's callsign (who sent this message).
     * @return The "from" field (usually a short, uppercase string).
     */
    const std::string& get_from() const;

    /**
     * @brief Set the sender's callsign ("from" field).
     * @param from The new sender ID (will be used as the message source).
     */
    void set_from(const std::string& from);

    /**
     * @brief Get the recipient's callsign (who this message is addressed to).
     * @return The "to" field (short string, e.g. "NODE2").
     */
    const std::string& get_to() const;

    /**
     * @brief Set the recipient's callsign ("to" field).
     * @param to The new recipient ID.
     */
    void set_to(const std::string& to);

    /**
     * @brief Get the main data payload (actual user/content message).
     * @return The "data" field (arbitrary string).
     */
    const std::string& get_data() const;

    /**
     * @brief Set the main data payload ("data" field).
     * @param data The new message content/body.
     */
    void set_data(const std::string& data);

    /**
     * @brief Check if this message is "valid" (all critical fields set).
     * @return True if the message is ready for sending/processing, false if incomplete.
     * @details
     *   Generally checks for non-empty sender/recipient and a valid header.
     */
    bool is_valid() const;

    /**
     * @brief Check if this message is a fragment (part of a multi-part message).
     * @return True if total > 1 (i.e., this is a split/fragmented message).
     */
    bool is_fragmented() const;

    /**
     * @brief Check if this message is a complete, unfragmented message.
     * @return True if total == 1 and part == 0 (single part only).
     */
    bool is_complete() const;

    /**
     * @brief Compare this message to another for equality (all fields).
     * @param other The message to compare against.
     * @return True if all header fields, sender, recipient, and data are identical.
     */
    bool operator==(const Message& other) const;

    /**
     * @brief Convert the message to the official on-the-wire string format.
     * @return Protocol string: "[HEADER_HEX]~[FROM]~[TO]~[DATA]".
     * @details
     *   Suitable for transmission over LoRa, serial, or logging.
     */
    std::string to_wire_string() const;

    /**
     * @brief Parse and populate fields from a wire-format string.
     * @param wire_str The protocol string ("4F2B000131~NODE1~NODE2~Hello").
     * @details
     *   Clears all current data, then fills from the parsed string.
     */
    void from_wire_string(const std::string& wire_str);

    /**
     * @brief Pack the message header to a binary buffer (for wire use).
     * @param out_buf Pointer to at least 5 bytes to fill.
     * @param buf_len Buffer length (should be >= 5).
     */
    void pack(uint8_t* out_buf, size_t buf_len) const;

    /**
     * @brief Unpack and fill header fields from a binary buffer.
     * @param in_buf Pointer to buffer with at least 5 bytes.
     * @param len Length of the buffer.
     */
    void unpack(const uint8_t* in_buf, size_t len);

    /**
     * @brief Produce a human-readable string describing this message.
     * @return Readable summary, including header, sender, recipient, and data.
     * @details
     *   Useful for debugging, logging, or inspection (not sent over the network).
     */
    std::string to_string() const;

private:
    MessageID id;
    std::string from;
    std::string to;
    std::string data;
};

}
