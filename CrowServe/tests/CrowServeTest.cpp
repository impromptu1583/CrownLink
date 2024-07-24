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
    auto net_addresses = GENERATE(
        NetAddress{"0123456789abcde"},
        NetAddress{"3333333"},
        NetAddress{""},
        NetAddress{}
    );
    REQUIRE(test_serialization(net_addresses));

    auto games = GENERATE(
        game{1, 2, 3, NetAddress{"0123456789abcde"},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}
    );
    REQUIRE(test_serialization(games));

    auto adfiles = GENERATE(
        AdFile{{1, 2, 3, NetAddress{"0123456789abcde"},4,5,6,"test name","test description",nullptr,nullptr,7,8,9},
            "test extra bytes", CrownLinkMode::CLNK},
        AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9},
            "", CrownLinkMode::DBCL},
        AdFile{}
    );
    REQUIRE(test_serialization(adfiles));
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
    INFO("connecting sender");
    auto succeeded = false;
    while (!succeeded) {
        succeeded = sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        std::this_thread::sleep_for(1s);
    }

    INFO("Connecting receiver");
    succeeded = false;
    while (!succeeded) {
        succeeded = receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    }

    INFO("Waiting for advertisement to be passed");
    std::unique_lock<std::mutex> lock{ad_mtx};
    ad_cv.wait(lock);
    INFO("comparing adfiles, name1: " << adfile.game_info.game_name << " name2: " << adfile_out.game_info.game_name);
    REQUIRE(adfile == adfile_out);
}

TEST_CASE("CrowServe integration") {
    auto id = NetAddress{};
    auto adfile = AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}, "test extra bytes", CrownLinkMode::CLNK};

    CrowServe::Socket crow_serve;
    crow_serve.listen(
        [](const CrownLink::ConnectionRequest &message) {
            INFO("Received ConnectionRequest");
        },
        [](const CrownLink::KeyExchange &message) {
            INFO("Received KeyExchange");
        },
        [&id](const CrownLink::ClientProfile &message) {
            INFO("Received ClientProfile");
            id = message.peer_id;
        },
        [](const CrownLink::UpdateAvailable &message) {
            INFO("Received UpdateAvailable");
        },
        [](const CrownLink::StartAdvertising &message) {
            INFO("Received StartAdvertising");
        },
        [](const CrownLink::StopAdvertising &message) {
            INFO("Received StopAdvertising");
        },
        [&crow_serve, &adfile](const CrownLink::AdvertisementsRequest &message) {
            INFO("Received AdvertisementsRequest");
            auto msg = CrownLink::StartAdvertising{{}, adfile};
            crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },
        [](const CrownLink::AdvertisementsResponse &message) {
            INFO("Received AdvertisementsResponse");
        },
        [](const CrownLink::EchoRequest &message) {
            INFO("Received EchoRequest");
        },
        [](const CrownLink::EchoResponse &message) {
            INFO("Received EchoResponse");
        },
        [](const auto &message) {}
    );
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);
    auto msg = CrownLink::ConnectionRequest{};
    crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    std::this_thread::sleep_for(5s);

}
