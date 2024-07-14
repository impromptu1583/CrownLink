#include <catch2/catch_all.hpp>

#include "CrowServe.h"

template<typename T>
bool test_serialization(T& test_subject) {
    Json first_json = test_subject;
    auto output = first_json.template get<T>();
    Json second_json = output;
    return first_json == second_json;
}

TEST_CASE("CBOR de/serialization") {
    auto net_address_test = NetAddress{"0123456789abcde"};
    REQUIRE(test_serialization(net_address_test));

    auto game_test = game{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9};
    REQUIRE(test_serialization(game_test));

    auto adfile_test = AdFile{game_test, "test extra bytes", CrownLinkMode::CLNK};
    REQUIRE(test_serialization(adfile_test));
}

TEST_CASE("CrowServe integration") {
    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    REQUIRE(crow_serve.receive());
}
