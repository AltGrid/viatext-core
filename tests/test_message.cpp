#include "viatext/message.hpp"
#include <iostream>

int main() {
    viatext::Message msg("12345", "alice", "bob", "hello world", 5);

    std::cout << "Original: " << msg.to_string() << "\n";

    msg.encrypt("key");
    std::cout << "Encrypted: " << msg.to_string() << "\n";

    msg.decrypt("key");
    std::cout << "Decrypted: " << msg.to_string() << "\n";

    // Test parsing
    auto parsed = viatext::Message::from_string(msg.to_string());
    if (parsed && parsed->is_valid())
        std::cout << "Parsed OK: " << parsed->payload << "\n";
    else
        std::cout << "Parse failed\n";
}
