#include <catch2/catch_all.hpp>

#include "CrowServe.h"

TEST_CASE("CrowServe integration") {
    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    REQUIRE(crow_serve.receive());
}
