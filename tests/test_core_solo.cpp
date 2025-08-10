#include <doctest/doctest.h>
#include "viatext/core.hpp"

using namespace viatext;

TEST_CASE("Core constructs with node id and exposes getters") {
    Core::NodeIdStr id = "NODE1";
    Core core{id};
    CHECK(core.get_node_id() == Core::NodeIdStr("NODE1"));
    CHECK(core.hops_max() == Core::HOPS_MAX_DEFAULT);
    CHECK(core.frag_cap()  == Core::FRAG_CAP_DEFAULT);
    CHECK(core.tick_count() == 0);
    CHECK(core.uptime_ms() == 0);
}

TEST_CASE("Core tick() advances counters and uptime monotonically") {
    Core core{Core::NodeIdStr("N1")};

    // first tick establishes last_ms_
    core.tick(1000);
    CHECK(core.tick_count() == 1);
    CHECK(core.uptime_ms() == 0); // first call sets baseline; delta=0

    // advance 250 ms
    core.tick(1250);
    CHECK(core.tick_count() == 2);
    CHECK(core.uptime_ms() == 250);

    // non-decreasing time keeps increasing uptime
    core.tick(2250);
    CHECK(core.uptime_ms() == 1250);

    // time going "backwards" should not underflow (code guards to 0 delta)
    core.tick(2000);
    CHECK(core.uptime_ms() == 1250);
}

TEST_CASE("Core set_node_id() updates the id") {
    Core core{Core::NodeIdStr("A")};
    core.set_node_id(Core::NodeIdStr("B"));
    CHECK(core.get_node_id() == Core::NodeIdStr("B"));
}

TEST_CASE("Core outbox is empty by default; get_message returns false") {
    Core core{Core::NodeIdStr("N")};
    viatext::Package out{}; // type is declared but we won't depend on its internals
    CHECK(core.get_message(out) == false);
}

TEST_CASE("Policy setters: hops_max and frag_cap can be adjusted") {
    Core core{Core::NodeIdStr("N")};
    core.set_hops_max(3);
    core.set_frag_cap(5);
    CHECK(core.hops_max() == 3);
    CHECK(core.frag_cap() == 5);
}
