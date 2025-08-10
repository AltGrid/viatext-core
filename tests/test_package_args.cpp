#include <doctest/doctest.h>
#include "viatext/package.hpp"

using namespace viatext;

TEST_CASE("ArgList set/replace/has/get/flag") {
    Package p;

    // set with trimming
    CHECK(p.args.set("   -rssi\t", "  -92  "));
    CHECK(p.args.set_flag("-m"));
    CHECK(p.args.has("-rssi"));
    CHECK(p.args.has("-m"));

    const ValStr* v = p.args.get("-rssi");
    REQUIRE(v != nullptr);
    CHECK(*v == ValStr("-92"));

    // replace existing value
    CHECK(p.args.set("-rssi", "-90"));
    v = p.args.get("-rssi");
    REQUIRE(v != nullptr);
    CHECK(*v == ValStr("-90"));

    // flags are empty strings by design
    const ValStr* fv = p.args.get("-m");
    REQUIRE(fv != nullptr);
    CHECK(fv->empty());

    // remove
    CHECK(p.args.remove("-rssi"));
    CHECK_FALSE(p.args.has("-rssi"));
}
