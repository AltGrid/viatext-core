#include <doctest/doctest.h>
#include "viatext/core.hpp"
#include "viatext/message.hpp"
#include "viatext/message_id.hpp"
#include "viatext/package.hpp"

using namespace viatext;

static Package make_pkg_from_msg(const Message& m) {
    Package p;
    p.payload = m.to_payload_stamp_copy(); // "<hex10>~from~to~data"
    return p;
}

TEST_CASE("Message to this node emits ACK (if requested) and delivered event") {
    Core core{Core::NodeIdStr("NODE")};

    MessageID id(0x1234, 0, 1, 0, 0);
    id.set_request_acknowledgment(true);
    Message in{id, "SRC", "NODE", "hello"};

    Package in_pkg = make_pkg_from_msg(in);
    in_pkg.args.set_flag("-m");

    REQUIRE(core.add_message(in_pkg) == true);
    core.tick(1000);

    Package out{};
    REQUIRE(core.get_message(out) == true);
    Message m1{out}; // ACK is a full stamp; safe to parse

    CHECK(m1.flag("-ack") == true);
    CHECK(m1.to() == ToStr("SRC"));
    CHECK(m1.text() == BodyStr("ACK"));

    REQUIRE(core.get_message(out) == true);
    Message m2{out}; // delivered event is also stamped
    CHECK(m2.flag("-r") == true);
    CHECK(m2.to() == ToStr("NODE"));
    CHECK(m2.text() == BodyStr("hello"));

    CHECK(core.get_message(out) == false);
}

TEST_CASE("Ping produces a pong to sender, from core node id") {
    Core core{Core::NodeIdStr("A")};

    MessageID id(0x0042, 0, 1, 0, 0);
    Message in{id, "X", "Y", "ignored"};
    Package in_pkg = make_pkg_from_msg(in);
    in_pkg.args.set_flag("-p");

    REQUIRE(core.add_message(in_pkg) == true);
    core.tick(2000);

    Package out{};
    REQUIRE(core.get_message(out) == true);
    Message m{out}; // pong is a full stamp

    CHECK(m.flag("-pong") == true);
    CHECK(m.from() == FromStr("A"));
    CHECK(m.to()   == ToStr("X"));
    CHECK(m.text() == BodyStr("PONG"));
    CHECK(core.get_message(out) == false);
}

TEST_CASE("Set-id changes node id and emits -id_set event (payload is plain text)") {
    Core core{Core::NodeIdStr("OLD")};

    MessageID id(0x9999, 0, 1, 0, 0);
    Message in{id, "Ctl", "OLD", "N2"}; // body carries new id per MVP
    Package in_pkg = make_pkg_from_msg(in);
    in_pkg.args.set_flag("--set-id");

    REQUIRE(core.add_message(in_pkg) == true);
    core.tick(3000);

    // Node id updated
    CHECK(core.get_node_id() == Core::NodeIdStr("N2"));

    // The confirmation event is NOT a full stamp; inspect Package directly
    Package out{};
    REQUIRE(core.get_message(out) == true);

    CHECK(out.flag("-id_set") == true);
    CHECK(out.payload == Text255("ID_SET~N2"));

    CHECK(core.get_message(out) == false);
}
