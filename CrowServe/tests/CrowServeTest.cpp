#include <catch2/catch_all.hpp>

#include "CrowServe.h"

TEST_CASE("CrowServe integration") {
    CrowServe::init_sockets();
    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    REQUIRE(crow_serve.receive());
    CrowServe::deinit_sockets();
}
