#ifndef VIATEXT_MESSAGE_HPP
#define VIATEXT_MESSAGE_HPP

/**
 * @file message.hpp
 * @brief ViaText Message Class — Universal Protocol Message Container for All Node Types
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-07
 */

#include "message_id.hpp"
#include "text_fragments.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

// These can be tuned for MCU limits.
constexpr size_t FROM_LEN = 6;  ///< Max sender field length (e.g. "HCKRmn")
constexpr size_t TO_LEN   = 6;  ///< Max recipient field length
constexpr size_t DATA_FRAGS = 8; ///< Max data fragments per message
constexpr size_t FRAG_SIZE = 32; ///< Max bytes per fragment

/**
 * @class Message
 * @brief The universal protocol message container for the ViaText communication system (Arduino-safe).
 */
class Message {
public:
    // --- Fields (all public for zero-overhead struct-style use) ---
    MessageID id;
    etl::string<FROM_LEN> from;
    etl::string<TO_LEN> to;
    TextFragments<DATA_FRAGS, FRAG_SIZE> data;

    uint8_t error = 0; // 0 = OK, 1 = parse error, 2 = overflow, 3 = empty, 4 = fragment error

    // --- Constructors ---
    Message();
    Message(const MessageID& id_, const char* from_, const char* to_, const char* data_);
    Message(const char* wire_str);

    // --- Field setters ---
    void set_id(const MessageID& id_);
    void set_from(const char* from_);
    void set_to(const char* to_);
    void set_data(const char* data_);
    void clear();

    // --- Field getters ---
    const MessageID& get_id() const;
    const etl::string<FROM_LEN>& get_from() const;
    const etl::string<TO_LEN>& get_to() const;
    const TextFragments<DATA_FRAGS, FRAG_SIZE>& get_data() const;

    // --- Serialization ---
    void to_wire_string(char* out, size_t max_len) const;
    bool from_wire_string(const char* wire_str);

    // --- Validity, fragmentation, and flags ---
    bool is_valid() const;
    bool is_fragmented() const;
    bool is_complete() const;

    // --- Debugging/inspection ---
    void to_string(char* out, size_t max_len) const;
};

} // namespace viatext

#endif // VIATEXT_MESSAGE_HPP
