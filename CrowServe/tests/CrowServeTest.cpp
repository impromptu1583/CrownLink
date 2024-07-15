#include <catch2/catch_all.hpp>

#include "CrowServe.h"
#include "Cbor.h"

template<typename T>
bool test_serialization(T& test_subject) {
    Json first_json = test_subject;
    auto cbor = Json::to_cbor(first_json);
    auto second_json = Json::from_cbor(cbor);
    auto recreated_test_subject = second_json.template get<T>();
    Json third_json = recreated_test_subject;
    return first_json == third_json;
}

template<typename T>
bool test_serialization_strict(T& test_subject) {
    Json json = test_subject;
    auto cbor = Json::to_cbor(json);
    Json json2 = Json::from_cbor(cbor);
    auto output = json2.template get<T>();
    return test_subject == output;
}

template<typename T>
bool test_cbor(T& test_subject) {
    std::vector<u8> serialized{};
    serialize_cbor(test_subject, serialized);
    T deserialized{};
    deserialize_cbor_into(deserialized, serialized);
    return test_subject == deserialized;
}

TEST_CASE("CBOR de/serialization") {
    auto net_address_test = NetAddress{"0123456789abcde"};
    REQUIRE(test_serialization_strict(net_address_test));

    auto game_test = game{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9};
    REQUIRE(test_serialization_strict(game_test));

    auto adfile_test = AdFile{game_test, "test extra bytes", CrownLinkMode::CLNK};
    REQUIRE(test_serialization_strict(adfile_test));

    REQUIRE(test_cbor(net_address_test));
}

TEST_CASE("CrowServe integration") {
    CrowServe::Socket crow_serve;
    REQUIRE(crow_serve.try_init());
    REQUIRE(crow_serve.receive());
}
