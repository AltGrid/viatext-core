// src/message.cpp
#include "viatext/message.hpp"
#include <sstream>

namespace viatext {

// Helper for very basic "encryption" (XOR) -- not real security!
static std::string xor_cipher(const std::string& data, const std::string& key) {
    std::string out = data;
    for (size_t i = 0; i < data.size(); ++i)
        out[i] ^= key[i % key.size()];
    return out;
}


std::optional<Message> Message::from_string(const std::string& raw) {
    // Simple parse: stamp|from|to|ttl|encrypted|payload
    std::istringstream iss(raw);
    std::string stamp, from, to, ttl_s, enc_s, payload;

    if (!std::getline(iss, stamp, '|')) return std::nullopt;
    if (!std::getline(iss, from, '|')) return std::nullopt;
    if (!std::getline(iss, to, '|')) return std::nullopt;
    if (!std::getline(iss, ttl_s, '|')) return std::nullopt;
    if (!std::getline(iss, enc_s, '|')) return std::nullopt;
    if (!std::getline(iss, payload)) return std::nullopt;

    int ttl = std::stoi(ttl_s);
    bool encrypted = (enc_s == "1");
    return Message(stamp, from, to, payload, ttl, encrypted);
}

std::string Message::to_string() const {
    // Serialize as stamp|from|to|ttl|encrypted|payload
    std::ostringstream oss;
    oss << stamp << '|' << from << '|' << to << '|' << ttl << '|'
        << (encrypted ? "1" : "0") << '|' << payload;
    return oss.str();
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
    // For now: must have non-empty stamp, from, payload
    return !stamp.empty() && !from.empty() && !payload.empty();
}

} // namespace viatext
