/**
 * @file message.hpp
 * @brief Protocol message structure for ViaText nodes (data only, no parsing).
 * @details
 *   This header defines the Message class—the fundamental unit exchanged between ViaText nodes.
 *   Each Message is a self-contained, portable data object representing one logical transmission
 *   (whether a mesh message, a directive, or an event/acknowledgment).
 *
 *   ## Design Goals
 *   - **Pure Data:** Message is a simple, serializable struct; all parsing, serialization, and protocol logic are
 *     delegated to the parser module or external wrappers.
 *   - **Minimalist Fields:** Only stores fields essential for ViaText routing, addressing, and application logic.
 *   - **No Ownership of Memory:** Uses only stack-allocated `std::string` and value types for portability.
 *   - **Portable and STL-Friendly:** Designed for maximum compatibility between desktop, embedded, and microcontroller builds.
 *   - **Explicitness:** Each field has a clear, documented meaning and type; no magic behavior or hidden state.
 *
 *   ## C++ Features
 *   - Uses in-class member initializers for clarity and safety.
 *   - Provides utility methods for basic validation and demonstration encryption.
 *   - Marked as non-virtual, copyable, and movable for maximum efficiency and simplicity.
 *
 *   ## Note on Parsing
 *   This file contains *no* JSON or protocol-specific parsing/serialization code.
 *   See `parser.hpp` for conversion between Message and on-the-wire formats.
 *
 *   ChatGPT assisted in the design and documentation of this file, with Leo as project lead and reviewer.
 *
 *   @author Leo
 *   @author ChatGPT
 *   @date   2025-08-04
 *   @copyright MIT License
 */
#pragma once

#include <string>

namespace viatext {

/**
 * @class Message
 * @brief Data model for a single ViaText protocol message.
 * @details
 *   The Message class is a plain data container representing one logical communication unit in the ViaText
 *   protocol. It is intentionally designed to hold only field values—no parsing, serialization, or network
 *   code is present in this class. All logic related to encoding, decoding, or protocol handling is delegated
 *   to the parser or higher-level wrappers.
 *
 *   ## Role in the System
 *   - Serves as the primary object exchanged between all nodes and system components.
 *   - Used by the core logic, parser, CLI, and any wrapper/bridge modules.
 *   - Designed for clarity: each member variable has a strict, documented purpose.
 *
 *   ## Design Features
 *   - **All fields public**: Allows simple copying, assignment, or testing. No internal invariants other than validity.
 *   - **Non-virtual and lightweight**: There is no inheritance or vtable—maximizing performance and predictability.
 *   - **Supports simple demo encryption**: Methods provided for demonstration (not secure), easily replaced.
 *   - **Can be stack-allocated**: No dynamic memory or resource management needed.
 *
 *   ## Extending Message
 *   - If the protocol expands, add fields here and update the parser/wire format accordingly.
 *   - Validation helpers (`is_valid()`) should be kept in sync with new required fields.
 *
 *   @note
 *     See `parser.hpp` for all (de)serialization logic, and `core.hpp` for message queueing and event handling.
 */
class Message {
public:
    /**
     * @brief Unique stamp or identifier for this message.
     * @details
     *   Typically constructed from the node's ID and a timestamp or counter, ensuring uniqueness across the mesh.
     *   Used for deduplication, message tracking, and auditing.
     *   Type: `std::string` for flexibility (could be hex, base64, etc.).
     */
    std::string stamp;

    /**
     * @brief Sender's node ID.
     * @details
     *   Identifies the original source of the message within the mesh or network.
     *   Must be unique per node in a given deployment.
     *   Type: `std::string` for full ASCII/Unicode support.
     */
    std::string from;

    /**
     * @brief Destination node ID, or empty for broadcast messages.
     * @details
     *   If set, indicates the intended final recipient.
     *   If empty, the message is interpreted as a broadcast to all reachable nodes.
     */
    std::string to;

    /**
     * @brief Main payload or message body.
     * @details
     *   Contains user data, command, or event description as a plain string (may be further structured if needed).
     *   Can be encrypted for privacy or integrity (see `encrypted`).
     */
    std::string payload;

    /**
     * @brief Time To Live (TTL) or remaining hop count for the message.
     * @details
     *   Used for controlling how many times a message can be relayed in the network.
     *   Decremented at each relay; dropped when zero or negative.
     *   Type: `int` for explicit sign and overflow handling.
     */
    int ttl = 0;

    /**
     * @brief Indicates whether the payload is encrypted.
     * @details
     *   Set to `true` if the payload has been encrypted (with a symmetric or other cipher).
     *   Allows wrappers or recipients to determine whether decryption is needed.
     */
    bool encrypted = false;


    /**
     * @brief Default constructor (creates an empty, uninitialized message).
     * @details
     *   All fields are set to their respective default values:
     *   - `stamp`, `from`, `to`, `payload`: empty strings
     *   - `ttl`: 0
     *   - `encrypted`: false
     *
     *   This constructor is `= default`, meaning the compiler will auto-generate the constructor for you.
     *   This is preferred for simple, value-only classes and improves maintainability.
     */
    Message() = default;

    /**
     * @brief Full-value constructor.
     * @param stamp      Unique message ID (see member docs).
     * @param from       Sender's node ID.
     * @param to         Destination node ID (or empty for broadcast).
     * @param payload    The main message content.
     * @param ttl        Time To Live / hop count (default: 0).
     * @param encrypted  Whether the payload is encrypted (default: false).
     * @details
     *   Allows creation of a complete Message object in a single line.
     *   All fields are copied or assigned directly.
     *   This pattern is idiomatic in C++ for “struct-like” classes where initialization of all fields is desired.
     */   
    Message(const std::string& stamp,
            const std::string& from,
            const std::string& to,
            const std::string& payload,
            int ttl = 0,
            bool encrypted = false)
        : stamp(stamp), from(from), to(to), payload(payload), ttl(ttl), encrypted(encrypted) {}

    // --- Utility Methods (no parsing/serialization) ---

    /**
     * @brief Checks if the message is structurally valid.
     * @return true if required fields are non-empty and conform to protocol; false otherwise.
     * @details
     *   This method performs a minimal integrity check, typically verifying:
     *    - `stamp`, `from`, and `payload` are non-empty.
     *    - Other checks can be added as the protocol evolves (e.g., valid node IDs, TTL bounds).
     *   Should be called before processing or transmitting a message.
     */
    bool is_valid() const;

    /**
     * @brief Encrypts the payload using a simple XOR cipher (demo only).
     * @param key Symmetric key used for encryption (same for encrypt/decrypt).
     * @details
     *   This method is *not secure*—it is for demonstration and testing only.
     *   If `encrypted` is already true, does nothing.
     *   For production or serious privacy, replace this with a cryptographically secure algorithm.
     *   Sets the `encrypted` flag to true.
     */
    void encrypt(const std::string& key);

    /**
     * @brief Decrypts the payload using a simple XOR cipher (demo only).
     * @param key Symmetric key used for decryption (must match encryption key).
     * @details
     *   If `encrypted` is false, does nothing.
     *   Sets the `encrypted` flag to false after successful decryption.
     *   Not secure—use only for educational or test purposes!
     */   
    void decrypt(const std::string& key);

private:
    /**
     * @brief Helper function for XOR-based encryption and decryption.
     * @param data Input string to be encrypted or decrypted.
     * @param key  Symmetric key to apply (cycled if shorter than data).
     * @return Resulting string after XOR operation.
     * @details
     *   This method is private and only used internally by `encrypt` and `decrypt`.
     *   Not cryptographically secure!
     */
    static std::string xor_cipher(const std::string& data, const std::string& key);
};

} // namespace viatext
