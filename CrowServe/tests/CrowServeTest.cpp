#include <catch2/catch_all.hpp>

#include "CrowServe.hpp"

TEST_CASE("CrowServe initialization") {
    auto crow_serve = CrowServeSocket{};
    REQUIRE(crow_serve.try_init());
    
}