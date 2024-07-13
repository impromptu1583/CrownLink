#include <catch2/catch_all.hpp>

#include "CrowServe.h"

TEST_CASE("CrowServe integration") {
    // test NetAddress CBOR
    auto in = NetAddress{};
    Json j = in;
    auto out = j.template get<NetAddress>();
    REQUIRE(in == out);

    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    REQUIRE(crow_serve.receive());
}
