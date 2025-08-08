#include "viatext/message.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include <stdio.h>
#include <string.h>

namespace viatext {

// --- Default constructor: zero/init all fields
Message::Message()
    : id(), from(), to(), data(), error(3) // error=3 means empty
{}

// --- Full-field constructor (all const char* for MCU/ETL)
Message::Message(const MessageID& id_, const char* from_, const char* to_, const char* data_)
    : id(id_), from(from_), to(to_), data(), error(0)
{
    set_from(from_);
    set_to(to_);
    set_data(data_);
}

// --- Parse from wire-format string (tilde-delimited)
Message::Message(const char* wire_str)
    : id(), from(), to(), data(), error(3)
{
    from_wire_string(wire_str);
}

// --- Setters
void Message::set_id(const MessageID& id_) { id = id_; }

void Message::set_from(const char* from_) {
    from.clear();
    if (from_) from.assign(from_, FROM_LEN);
}
void Message::set_to(const char* to_) {
    to.clear();
    if (to_) to.assign(to_, TO_LEN);
}
void Message::set_data(const char* data_) {
    data.set(data_);
    if (data.error != 0){ 
        error = 2; 
    } else {
        error = 0;
    }
}

// --- Getters
const MessageID& Message::get_id() const { return id; }
const etl::string<FROM_LEN>& Message::get_from() const { return from; }
const etl::string<TO_LEN>& Message::get_to() const { return to; }
const TextFragments<DATA_FRAGS, FRAG_SIZE>& Message::get_data() const { return data; }

// --- Serialization to wire-format string ("HEADER~FROM~TO~DATA")
void Message::to_wire_string(char* out, size_t max_len) const {
    char hexbuf[11] = {0};
    uint8_t packed_id[5];
    id.pack(packed_id);

    // Convert header to hex string
    for (int i = 0; i < 5; ++i) {
        snprintf(hexbuf + i * 2, 3, "%02X", packed_id[i]);
    }

    // Data: flatten to one string for wire (fragment join)
    etl::string<DATA_FRAGS * FRAG_SIZE> flat;
    for (uint8_t i = 0; i < data.used_fragments; ++i) flat.append(data.fragments[i]);

    // Compose full message: "HEADER~FROM~TO~DATA"
    snprintf(out, max_len, "%s~%s~%s~%s", hexbuf, from.c_str(), to.c_str(), flat.c_str());
}

// --- Parsing from wire-format string ("HEADER~FROM~TO~DATA")
bool Message::from_wire_string(const char* wire_str) {
    clear();
    error = 3; // Assume empty until proven otherwise

    // Step 1: Split on tilde (~) up to 4 fields
    const char* fields[4] = {nullptr, nullptr, nullptr, nullptr};
    int field_idx = 0;
    fields[0] = wire_str;
    for (const char* p = wire_str; *p && field_idx < 3; ++p) {
        if (*p == '~') {
            fields[++field_idx] = p + 1;
            // Null-terminate previous field (hack: const_cast ok for ETL/Arduino context)
            *((char*)p) = '\0';
        }
    }
    if (field_idx < 3) { error = 1; return false; } // Not enough fields

    // Step 2: Parse header
    if (strlen(fields[0]) == 10) {
        uint8_t packed_id[5] = {0};
        for (int i = 0; i < 5; ++i) {
            char byte_str[3] = {fields[0][i * 2], fields[0][i * 2 + 1], 0};
            packed_id[i] = (uint8_t)strtoul(byte_str, nullptr, 16);
        }
        id.unpack(packed_id, 5);
    } else {
        error = 1; return false; // Invalid header
    }

    // Step 3: Parse from, to, data (truncate if needed)
    set_from(fields[1]);
    set_to(fields[2]);
    set_data(fields[3]);
    error = 0;
    return true;
}

// --- Validity check
bool Message::is_valid() const {
    return (from.size() > 0 && to.size() > 0 && data.used_fragments > 0 && id.sequence > 0);
}
bool Message::is_fragmented() const { return id.total > 1; }
bool Message::is_complete() const { return id.part == 0 && id.total == 1; }

// --- Human-readable (debug) string
void Message::to_string(char* out, size_t max_len) const {
    char id_str[64];
    id.to_string(id_str, sizeof(id_str));
    snprintf(out, max_len, "%s FROM:%s TO:%s DATA:%s", id_str, from.c_str(), to.c_str(), data.fragments[0].c_str());
}

void Message::clear() {
    id = MessageID();
    from.clear();
    to.clear();
    data.clear(); // Assuming you implemented clear() for TextFragments
    error = 3;
}


} // namespace viatext
