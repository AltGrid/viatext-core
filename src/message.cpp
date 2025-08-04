/**
 * @file message.cpp
 * @brief Implementation of ViaText Message class (utility methods and demo encryption).
 * @details
 *   Implements member functions declared in message.hpp. See class docs for design and rationale.
 *   ChatGPT assisted in drafting this code and comments.
 *
 *   @author Leo
 *   @author ChatGPT
 *   @date   2025-08-04
 */

#include "viatext/message.hpp"

namespace viatext {

// XOR "encryption" helper (demo only)
std::string Message::xor_cipher(const std::string& data, const std::string& key) {
    std::string out = data;
    for (size_t i = 0; i < data.size(); ++i)
        out[i] ^= key[i % key.size()];
    return out;
}

void Message::encrypt(const std::string& key) {
    if (!encrypted) {
        payload = xor_cipher(payload, key);
        encrypted = true;
    }
}

void Message::decrypt(const std::string& key) {
    if (encrypted) {
        payload = xor_cipher(payload, key);
        encrypted = false;
    }
}

bool Message::is_valid() const {
    return !stamp.empty() && !from.empty() && !payload.empty();
}

} // namespace viatext
