#include <doctest/doctest.h>
#include "viatext/core.hpp"
#include "viatext/message.hpp"
#include "viatext/message_id.hpp"
#include "viatext/package.hpp"

using namespace viatext;

static Package stamp(MessageID id, const char* from, const char* to, const char* data, const char* flag) {
    Message m{id, from, to, data};
    Package p;
    p.payload = m.to_payload_stamp_copy();
    if (flag && *flag) p.args.set_flag(flag);
    return p;
}

TEST_CASE("TTL policy: messages with hops > hops_max are dropped") {
    Core core{Core::NodeIdStr("Z")};
    core.set_hops_max(3);

    MessageID id(0x0101, 0, 1, /*hops*/4, /*flags*/0); // 4 > 3
    Package in = stamp(id, "A", "Z", "msg", "-m");
    REQUIRE(core.add_message(in) == true);

    core.tick(100);
    Package out{};
    CHECK(core.get_message(out) == false); // dropped by policy
}

TEST_CASE("Dedupe: same sequence is processed once") {
    Core core{Core::NodeIdStr("A")};

    // Use ping so it always yields a response regardless of addressing
    MessageID id(0x7777, 0, 1, 0, 0);
    Package p1 = stamp(id, "X", "Y", "x", "-p");
    Package p2 = stamp(id, "X", "Y", "y", "-p"); // same sequence

    REQUIRE(core.add_message(p1) == true);
    REQUIRE(core.add_message(p2) == true);

    core.tick(10);   // processes first
    core.tick(20);   // second ignored due to dedupe

    Package out{};
    REQUIRE(core.get_message(out) == true);  // exactly one pong
    CHECK(core.get_message(out) == false);
}

TEST_CASE("Fragment policy (MVP): fragments are stored, not dispatched") {
    Core core{Core::NodeIdStr("N")};

    MessageID id(0x2222, /*part*/0, /*total*/2, /*hops*/0, /*flags*/0);
    Package in = stamp(id, "A", "N", "partial", "-m");
    REQUIRE(core.add_message(in) == true);

    core.tick(50);
    Package out{};
    CHECK(core.get_message(out) == false);
}
