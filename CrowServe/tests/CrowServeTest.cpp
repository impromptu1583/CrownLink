#include <catch2/catch_all.hpp>

#include "CrowServe.h"

TEST_CASE("CBOR de/serialization") {
    auto in = NetAddress{};
    Json j = in;
    auto out = j.template get<NetAddress>();
    REQUIRE(in == out);

    auto game_in = game{
        1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9
    };
    Json game_j = game_in;
    std::cout << game_j << std::endl;
    auto game_out = game_j.template get<game>();
    Json game_j2 = game_out;
    std::cout << game_j2 << std::endl;
    REQUIRE(game_j == game_j2);
}

TEST_CASE("CrowServe integration") {
    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    //REQUIRE(crow_serve.receive());
}
