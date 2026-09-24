#include <catch2/catch_all.hpp>

#include "shared/StormTypes.h"
#include "shared/Cbor.h"

template<typename T>
bool test_serialization(T& test_subject) {
    std::vector<u8> serialized{};
    serialize_cbor(test_subject, serialized);
    T deserialized{};
    deserialize_cbor_into(deserialized, serialized);
    return test_subject == deserialized;
}

TEST_CASE("CBOR de/serialization") {
    auto net_addresses = GENERATE(
        NetAddress{"0123456789abcde"},
        NetAddress{"3333333"},
        NetAddress{""},
        NetAddress{}
    );
    REQUIRE(test_serialization(net_addresses));

    auto games = GENERATE(
        GameInfo{1, 2, 3, NetAddress{"0123456789abcde"},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}
    );
    REQUIRE(test_serialization(games));

    auto adfiles = GENERATE(
        AdFile{{1, 2, 3, NetAddress{"0123456789abcde"},4,5,6,"test name","test description",nullptr,nullptr,7,8,9},
            "test extra bytes", std::to_underlying(TurnsPerSecond::Standard)},
        AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9},
            "", std::to_underlying(TurnsPerSecond::UltraLow)},
        AdFile{}
    );
    REQUIRE(test_serialization(adfiles));
}