#include <doctest/doctest.h>
#include "viatext/message_id.hpp"

using namespace viatext;

TEST_CASE("MessageID field ctor and hex round-trip") {
    MessageID id_in(0xBEEF, 7, 9, 3, 0);
    id_in.set_request_acknowledgment(true);
    id_in.set_is_encrypted(true); // flags now: RA + ENC

    auto hex = id_in.to_hex_string();
    CHECK(hex.size() == 10);

    // Construct back from hex
    MessageID id_hex(hex.c_str());
    // to_hex_string() should match (canonical uppercase)
    CHECK(id_hex.to_hex_string() == hex);

    // Raw fields should match (nibbles clamped by your implementation as needed)
    CHECK(id_hex.sequence == 0xBEEF);
    CHECK(id_hex.part     == 7);
    CHECK(id_hex.total    == 9);
    CHECK((id_hex.hops & 0x0F) == 3);
    CHECK(id_hex.requests_acknowledgment() == true);
    CHECK(id_hex.is_encrypted() == true);
    CHECK(id_hex.is_acknowledgment() == false);
}

TEST_CASE("MessageID pack/unpack preserves bytes") {
    MessageID a(0x1234, 0, 1, 0xA, 0xC); // example from docs: hops=10, flags=0xC
    uint8_t buf[5]{};
    a.pack(buf);

    MessageID b;
    b.unpack(buf, 5);

    CHECK(b.sequence == 0x1234);
    CHECK(b.part == 0);
    CHECK(b.total == 1);
    CHECK((b.hops & 0x0F) == 0xA);
    CHECK((b.flags & 0x0F) == 0xC);
}
