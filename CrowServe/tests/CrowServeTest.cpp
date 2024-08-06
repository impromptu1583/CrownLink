#include <catch2/catch_all.hpp>

#include "CrowServe.h"
#include "Cbor.h"
#include <condition_variable>

template <typename T>
struct ReceiveWrapper {
    T destination;
    std::mutex mtx;
    std::condition_variable cv;
    void wait_until_updated() {
        std::unique_lock lock{mtx};
        cv.wait(lock);
    }
};

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
    ReceiveWrapper<AdFile> adfile_out;

    CrowServe::Socket sender_socket;
    CrowServe::Socket receiver_socket;

    sender_socket.listen(
        [&sender_socket, &adfile](const CrownLinkProtocol::AdvertisementsRequest &message) {
            auto msg = CrownLinkProtocol::StartAdvertising{{}, adfile};
            sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },        
        [](const auto &message) {}        
    );

    receiver_socket.listen(
        [&adfile_out](const CrownLinkProtocol::AdvertisementsResponse &message) {
            if (!message.ad_files.empty()) {
                std::lock_guard lock{adfile_out.mtx};
                adfile_out.destination = message.ad_files[0];
                adfile_out.cv.notify_all();
            }
        },
        [&receiver_socket](const CrownLinkProtocol::AdvertisementsRequest &message) {
            auto msg = CrownLinkProtocol::AdvertisementsRequest{};
            receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },        
        [](const auto &message) {}
    );
    
    auto msg = CrownLinkProtocol::ConnectionRequest{};
    INFO("connecting sender");
    auto succeeded = false;
    while (!succeeded) {
        std::this_thread::sleep_for(1s);
        succeeded = sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    }

    INFO("Connecting receiver");
    succeeded = false;
    while (!succeeded) {
        std::this_thread::sleep_for(1s);
        succeeded = receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    }

    INFO("Waiting for advertisement to be passed");
    adfile_out.wait_until_updated();
    INFO("comparing adfiles, name1: " << adfile.game_info.game_name << " name2: " << adfile_out.destination.game_info.game_name);
    REQUIRE(adfile == adfile_out.destination);
}

TEST_CASE("P2P message exchange") {
    ReceiveWrapper<P2P::Pong> output;
    ReceiveWrapper<NetAddress> receiver_id;
    ReceiveWrapper<NetAddress> sender_id;    

    CrowServe::Socket sender_socket;
    CrowServe::Socket receiver_socket;

    receiver_socket.listen(
        [&receiver_id](const CrownLinkProtocol::ClientProfile &message) {
            std::lock_guard lock(receiver_id.mtx);
            receiver_id.destination = message.peer_id;
            receiver_id.cv.notify_all();
        },
        [&receiver_socket](const P2P::Ping &message) {
            auto msg = P2P::Pong{{message.header.peer_id},message.timestamp};
            receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, msg);
        },        
        [](const auto &message) {}        
    );
    sender_socket.listen(
        [&sender_id](const CrownLinkProtocol::ClientProfile &message) {
            std::lock_guard lock{sender_id.mtx};
            sender_id.destination = message.peer_id;
            sender_id.cv.notify_all();
        },
        [&output](const P2P::Pong &message) {
            std::lock_guard lock{output.mtx};
            output.destination = message;
            output.cv.notify_all();
        },        
        [](const auto &message) {}   
    );

    auto msg = CrownLinkProtocol::ConnectionRequest{};

    INFO("connecting sender");
    auto succeeded = false;
    while (!succeeded) {
        std::this_thread::sleep_for(1s);
        succeeded = sender_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    }
    sender_id.wait_until_updated();
    INFO("Received sender id " << sender_id.destination);


    INFO("Connecting receiver");
    succeeded = false;
    while (!succeeded) {
        std::this_thread::sleep_for(1s);
        succeeded = receiver_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);

    }
    receiver_id.wait_until_updated();
    INFO("Received receiver id " << receiver_id.destination);


    auto ping = P2P::Ping{{receiver_id.destination},"TestMessage"};
    INFO("Sending PING to " << ping.header.peer_id.b64());
    sender_socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, ping);

    INFO("Waiting for reply");
    std::unique_lock<std::mutex> out_lock{output.mtx};
    output.cv.wait(out_lock);

    INFO("Received reply message:" << output.destination.timestamp);

    REQUIRE(output.destination.timestamp == "TestMessage");
}


TEST_CASE("CrowServe integration") {
    auto id = NetAddress{};
    auto adfile = AdFile{{1, 2, 3, NetAddress{},4,5,6,"test name","test description",nullptr,nullptr,7,8,9}, "test extra bytes", CrownLinkMode::CLNK};

    CrowServe::Socket crow_serve;
    crow_serve.listen(
        [](const CrownLinkProtocol::ConnectionRequest &message) {
            INFO("Received ConnectionRequest");
        },
        [](const CrownLinkProtocol::KeyExchange &message) {
            INFO("Received KeyExchange");
        },
        [&id](const CrownLinkProtocol::ClientProfile &message) {
            INFO("Received ClientProfile");
            id = message.peer_id;
        },
        [](const CrownLinkProtocol::UpdateAvailable &message) {
            INFO("Received UpdateAvailable");
        },
        [&crow_serve, &adfile](const CrownLinkProtocol::AdvertisementsRequest &message) {
            INFO("Received AdvertisementsRequest");
            auto msg = CrownLinkProtocol::StartAdvertising{{}, adfile};
            crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
        },
        [](const CrownLinkProtocol::AdvertisementsResponse &message) {
            INFO("Received AdvertisementsResponse");
        },
        [](const CrownLinkProtocol::EchoRequest &message) {
            INFO("Received EchoRequest");
        },
        [](const CrownLinkProtocol::EchoResponse &message) {
            INFO("Received EchoResponse");
        },
        [](const auto &message) {}
    );
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2s);
    auto msg = CrownLinkProtocol::ConnectionRequest{};
    crow_serve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, msg);
    std::this_thread::sleep_for(5s);

}
