#include <catch2/catch_all.hpp>

#include "CrowServe.h"
#include "Cbor.h"

template<typename T>
bool test_serialization(T& test_subject) {
    std::vector<u8> serialized{};
    serialize_cbor(test_subject, serialized);
    T deserialized{};
    deserialize_cbor_into(deserialized, serialized);
    return test_subject == deserialized;
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
    crow_serve.listen(CrowServe::Overloaded{
        [](const CrownLink::ConnectionRequest &message) {
            std::cout << "Received ConnectionRequest\n";
        },
        [](const CrownLink::KeyExchange &message) {
            std::cout << "Received KeyExchange\n";
        },
        [](const CrownLink::ClientProfile &message) {
            std::cout << "Received ClientProfile\n";
        },
        [](const CrownLink::UpdateAvailable &message) {
            std::cout << "Received UpdateAvailable\n";
        },
        [](const CrownLink::StartAdvertising &message) {
            std::cout << "Received StartAdvertising\n";
        },
        [](const CrownLink::StopAdvertising &message) {
            std::cout << "Received StopAdvertising\n";
        },
        [](const CrownLink::AdvertisementsRequest &message) {
            std::cout << "Received AdvertisementsRequest\n";
        },
        [](const CrownLink::AdvertisementsResponse &message) {
            std::cout << "Received AdvertisementsResponse\n";
        },
        [](const CrownLink::EchoRequest &message) {
            std::cout << "Received EchoRequest\n";
        },
        [](const CrownLink::EchoResponse &message) {
            std::cout << "Received EchoResponse\n";
        }
    });
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);
    auto msg = CrownLink::ConnectionRequest{};
    crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    std::this_thread::sleep_for(5s);

}
