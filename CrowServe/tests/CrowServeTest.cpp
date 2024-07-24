#include <catch2/catch_all.hpp>

#include "CrowServe.h"
#include "Cbor.h"
#include <condition_variable>

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

TEST_CASE("Advertisement Exchange") {
    auto adfile = AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}, "test extra bytes", CrownLinkMode::CLNK};
    auto adfile_out = AdFile{};

    std::mutex ad_mtx;
    std::condition_variable ad_cv;

    CrowServe::Socket sender_socket;
    CrowServe::Socket receiver_socket;

    sender_socket.listen(
        [&sender_socket, &adfile](const CrownLink::AdvertisementsRequest &message) {
            auto msg = CrownLink::StartAdvertising{{}, adfile};
            sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },        
        [](const auto &message) {}        
    );

    receiver_socket.listen(
        [&adfile_out, &ad_mtx, &ad_cv](const CrownLink::AdvertisementsResponse &message) {
            if (!message.ad_files.empty()) {
                std::lock_guard lock{ad_mtx};
                adfile_out = message.ad_files[0];
                ad_cv.notify_all();
            }
        },
        [&receiver_socket](const CrownLink::AdvertisementsRequest &message) {
            auto msg = CrownLink::AdvertisementsRequest{};
            receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },        
        [](const auto &message) {}
    );
    
    auto msg = CrownLink::ConnectionRequest{};
    std::cout << "connecting sender" << std::endl;
    auto succeeded = false;
    while (!succeeded) {
        succeeded = sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        std::this_thread::sleep_for(1s);
    }

    std::cout << "connecting receiver" << std::endl;
    succeeded = false;
    while (!succeeded) {
        succeeded = receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    }

    std::cout << "waiting for advertisement to be passed" << std::endl;
    std::unique_lock<std::mutex> lock{ad_mtx};
    ad_cv.wait(lock);
    std::cout << "comparing adfiles, name1: " << adfile.game_info.game_name << " name2: " << adfile_out.game_info.game_name << std::endl;
    REQUIRE(adfile == adfile_out);
}

TEST_CASE("CrowServe integration") {
    auto id = NetAddress{};
    auto adfile = AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}, "test extra bytes", CrownLinkMode::CLNK};

    CrowServe::Socket crow_serve;
    crow_serve.listen(
        [](const CrownLink::ConnectionRequest &message) {
            std::cout << "Received ConnectionRequest\n";
        },
        [](const CrownLink::KeyExchange &message) {
            std::cout << "Received KeyExchange\n";
        },
        [&id](const CrownLink::ClientProfile &message) {
            std::cout << "Received ClientProfile\n";
            id = message.peer_id;
            std::cout << id.b64() << std::endl;
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
        [&crow_serve, &adfile](const CrownLink::AdvertisementsRequest &message) {
            std::cout << "Received AdvertisementsRequest\n";
            auto msg = CrownLink::StartAdvertising{{}, adfile};
            crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },
        [](const CrownLink::AdvertisementsResponse &message) {
            std::cout << "Received AdvertisementsResponse\n";
        },
        [](const CrownLink::EchoRequest &message) {
            std::cout << "Received EchoRequest\n";
        },
        [](const CrownLink::EchoResponse &message) {
            std::cout << "Received EchoResponse\n";
        },
        [](const auto &message) {}
    );
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);
    auto msg = CrownLink::ConnectionRequest{};
    crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    std::this_thread::sleep_for(5s);

}
