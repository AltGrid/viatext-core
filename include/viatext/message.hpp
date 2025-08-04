// include/viatext/message.hpp
#pragma once

#include <string>
#include <optional>

namespace viatext {

// The Message class represents a single text protocol message.
// - It is the unit exchanged between nodes.
// - It handles parsing, serialization, and (optionally) encryption.

class Message {
public:
    // === Data members ===
    std::string stamp;        // Unique stamp (ID + timestamp or similar)
    std::string from;         // Sender's node ID
    std::string to;           // Destination node ID (or empty for broadcast)
    std::string payload;      // Actual message content
    int ttl = 0;              // Time to live / hop count (optional)
    bool encrypted = false;   // Indicates if payload is encrypted

    // === Constructors ===

    Message() = default;
    Message(const std::string& stamp,
            const std::string& from,
            const std::string& to,
            const std::string& payload,
            int ttl = 0,
            bool encrypted = false)
        : stamp(stamp), from(from), to(to), payload(payload), ttl(ttl), encrypted(encrypted) {}

    // === Parsing and Serialization ===

    // Parses a message from a raw string (returns nullopt if invalid)
    static std::optional<Message> from_string(const std::string& raw);

    // Serializes this message to a wire/text format
    std::string to_string() const;

    // === Encryption ===

    // Encrypts the payload with a basic XOR cipher for demonstration (NOT secure!)
    // Replace with real encryption later.
    void encrypt(const std::string& key);

    // Decrypts the payload (if encrypted) with the given key.
    void decrypt(const std::string& key);

    // === Validation ===

    // Checks if this message is structurally valid (required fields present)
    bool is_valid() const;

    // Add more methods as needed...
};

} // namespace viatext
