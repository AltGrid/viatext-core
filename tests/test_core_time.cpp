#include <doctest/doctest.h>
#include "viatext/core.hpp"
using namespace viatext;

TEST_CASE("Core constructs and exposes defaults") {
    Core core{Core::NodeIdStr("NODE1")};
    CHECK(core.get_node_id() == Core::NodeIdStr("NODE1"));
    CHECK(core.hops_max() == Core::HOPS_MAX_DEFAULT);
    CHECK(core.frag_cap() == Core::FRAG_CAP_DEFAULT);
    CHECK(core.tick_count() == 0);
    CHECK(core.uptime_ms() == 0);
}

TEST_CASE("Core tick() advances uptime monotonically; backwards time ignored") {
    Core core{Core::NodeIdStr("N1")};

    core.tick(1000);                 // baseline (delta=0)
    CHECK(core.tick_count() == 1);
    CHECK(core.uptime_ms() == 0);

    core.tick(1250);                 // +250
    CHECK(core.tick_count() == 2);
    CHECK(core.uptime_ms() == 250);

    core.tick(2250);                 // +1000
    CHECK(core.uptime_ms() == 1250);

    core.tick(2000);                 // backwards — ignored
    CHECK(core.uptime_ms() == 1250);
}

TEST_CASE("Policy setters: hops_max and frag_cap are writable") {
    Core core{Core::NodeIdStr("N")};
    core.set_hops_max(3);
    core.set_frag_cap(5);
    CHECK(core.hops_max() == 3);
    CHECK(core.frag_cap() == 5);
}
