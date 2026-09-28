#include <catch2/catch_all.hpp>

#include "shared/StormTypes.h"
#include "shared/Cbor.h"
#include "WireFixtures.h"

#include "crow_serve/CrownLinkProtocol.h"
#include "crow_serve/P2PProtocol.h"

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

namespace {

void require_wire_bytes(const auto& message, std::string_view expected_hex, std::string_view fixture_name) {
    std::vector<u8> wire_bytes{};
    serialize_cbor(message, wire_bytes);
    const auto actual_hex = wire_fixtures::to_hex(wire_bytes);
    if (actual_hex != expected_hex) {
        FAIL(std::format(
            "wire bytes for {} changed - this breaks wire compatibility with crowserve and older "
            "clients:\n  expected: {}\n  actual:   {}",
            fixture_name, expected_hex, actual_hex
        ));
    }
}

template <typename T>
T decoded(std::string_view hex) {
    T message{};
    auto bytes = wire_fixtures::from_hex(hex);
    REQUIRE(deserialize_cbor_into(message, bytes));
    return message;
}

template <typename T>
bool decode_fails(std::string_view hex) {
    auto bytes = wire_fixtures::from_hex(hex);
    auto message = T{};
    return !deserialize_cbor_into(message, bytes);
}

}  // namespace

TEST_CASE("Wire format golden bytes: CrownLink protocol") {
    namespace rw = wire_fixtures::reflected_wire;
    using namespace CrownLinkProtocol;

    require_wire_bytes(
        wire_fixtures::make_connection_request_full(), rw::connection_request_full, "connection_request_full"
    );
    require_wire_bytes(
        wire_fixtures::make_connection_request_minimal(), rw::connection_request_minimal, "connection_request_minimal"
    );
    require_wire_bytes(wire_fixtures::make_key_exchange(), rw::key_exchange, "key_exchange");
    require_wire_bytes(wire_fixtures::make_turn_server(), rw::turn_server, "turn_server");
    require_wire_bytes(wire_fixtures::make_ice_credentials(), rw::ice_credentials, "ice_credentials");
    require_wire_bytes(wire_fixtures::make_client_profile(), rw::client_profile, "client_profile");
    require_wire_bytes(wire_fixtures::make_update_available(), rw::update_available, "update_available");
    require_wire_bytes(wire_fixtures::make_start_advertising(), rw::start_advertising, "start_advertising");
    // the empty message is an empty CBOR map (bf ff) under reflect-cpp; the
    // crowserve tests pin that the server's decoder accepts it
    require_wire_bytes(wire_fixtures::make_stop_advertising(), rw::stop_advertising, "stop_advertising");
    require_wire_bytes(wire_fixtures::make_advertisements_request(), rw::advertisements_request, "advertisements_request");
    require_wire_bytes(
        wire_fixtures::make_advertisements_response(), rw::advertisements_response, "advertisements_response"
    );
    require_wire_bytes(wire_fixtures::make_echo_request(), rw::echo_request, "echo_request");
    require_wire_bytes(wire_fixtures::make_echo_response(), rw::echo_response, "echo_response");
}

TEST_CASE("Wire format golden bytes: P2P protocol") {
    namespace rw = wire_fixtures::reflected_wire;

    require_wire_bytes(wire_fixtures::make_p2p_connection_request(), rw::p2p_connection_request, "p2p_connection_request");
    require_wire_bytes(wire_fixtures::make_p2p_pong(), rw::p2p_pong, "p2p_pong");
    require_wire_bytes(
        wire_fixtures::make_p2p_juice_local_description(), rw::p2p_juice_local_description, "p2p_juice_local_description"
    );
    require_wire_bytes(wire_fixtures::make_p2p_juice_candidate(), rw::p2p_juice_candidate, "p2p_juice_candidate");
    require_wire_bytes(wire_fixtures::make_p2p_juice_done(), rw::p2p_juice_done, "p2p_juice_done");
}

TEST_CASE("Reflected fixtures decode back to their values") {
    using namespace CrownLinkProtocol;
    namespace rw = wire_fixtures::reflected_wire;

    const auto request = decoded<ConnectionRequest>(rw::connection_request_full);
    REQUIRE(request.preshared_key == "preshared");
    REQUIRE(request.peer_id_requested);
    REQUIRE(request.lobby_password == "hunter2");
    REQUIRE(request.product_id == 1);
    REQUIRE(request.version_id == 300);
    REQUIRE(request.crownlink_version == 0x12345678);

    const auto advertisement = decoded<StartAdvertising>(rw::start_advertising);
    REQUIRE(advertisement.ad == wire_fixtures::make_ad_file());
    REQUIRE(advertisement.lobby_password == "pw");

    const auto response = decoded<AdvertisementsResponse>(rw::advertisements_response);
    REQUIRE(response.ad_files.size() == 2);
    REQUIRE(response.ad_files[0] == wire_fixtures::make_ad_file());
    REQUIRE(response.ad_files[1] == AdFile{});

    const auto echo = decoded<EchoRequest>(rw::echo_request);
    REQUIRE(echo.message == "ping");

    const auto p2p_request = decoded<P2P::ConnectionRequest>(rw::p2p_connection_request);
    REQUIRE(p2p_request.header.agent_type == JuiceAgentType::P2P);
    REQUIRE(p2p_request.counter == 0x0a0b0c0d);
}

TEST_CASE("Legacy nlohmann-encoded fixtures still decode") {
    // frozen bytes from the previous serializer (definite-length maps, sorted
    // keys, stop_advertising as 0xf6 null); clients must keep reading these
    // shapes
    using namespace CrownLinkProtocol;
    namespace cw = wire_fixtures::crownlink_wire;

    const auto request = decoded<ConnectionRequest>(cw::connection_request_full);
    REQUIRE(request.preshared_key == "preshared");
    REQUIRE(request.peer_id_requested);
    REQUIRE(
        request.requested_id == wire_fixtures::make_address(
                                     {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}
                                 )
    );
    REQUIRE(
        request.request_token == wire_fixtures::make_address(
                                     {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff}
                                 )
    );
    REQUIRE(request.lobby_password == "hunter2");
    REQUIRE(request.product_id == 1);
    REQUIRE(request.version_id == 300);
    REQUIRE(request.crownlink_version == 0x12345678);

    const auto advertisement = decoded<StartAdvertising>(cw::start_advertising);
    REQUIRE(advertisement.ad == wire_fixtures::make_ad_file());
    REQUIRE(advertisement.lobby_password == "pw");

    const auto response = decoded<AdvertisementsResponse>(cw::advertisements_response);
    REQUIRE(response.ad_files.size() == 2);
    REQUIRE(response.ad_files[0] == wire_fixtures::make_ad_file());
    REQUIRE(response.ad_files[1] == AdFile{});

    const auto profile = decoded<ClientProfile>(cw::client_profile);
    REQUIRE(profile.ice_credentials.stun_host == "stun.example.com");
    REQUIRE(profile.ice_credentials.stun_port == "3478");

    const auto echo = decoded<EchoRequest>(cw::echo_request);
    REQUIRE(echo.message == "ping");

    const auto p2p_request = decoded<P2P::ConnectionRequest>(cw::p2p_connection_request);
    REQUIRE(p2p_request.header.agent_type == JuiceAgentType::P2P);
    REQUIRE(p2p_request.counter == 0x0a0b0c0d);
}

TEST_CASE("crowserve-produced fixtures decode despite different key order") {
    // fxamacker canonical CBOR sorts map keys length-first, unlike
    // reflect-cpp's declaration order; the C++ decoder must stay
    // order-agnostic. These fixtures are captured from the actual crowserve
    // encoder.
    using namespace CrownLinkProtocol;
    using namespace P2P;
    namespace sw = wire_fixtures::crowserve_wire;

    const auto profile = decoded<ClientProfile>(sw::client_profile);
    REQUIRE(profile.peer_id == wire_fixtures::make_address(
                                   {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}
                               ));
    REQUIRE(profile.token == wire_fixtures::make_address(
                                 {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f}
                             ));
    REQUIRE(profile.ice_credentials.stun_host == "stun.example.com");
    REQUIRE(profile.ice_credentials.stun_port == "3478");
    REQUIRE(profile.ice_credentials.turn_server_list().size() == 2);
    REQUIRE(profile.ice_credentials.turn_server_list()[0].host == "turn.example.com");
    REQUIRE(profile.ice_credentials.turn_server_list()[0].username == "user");
    REQUIRE(profile.ice_credentials.turn_server_list()[1].host == "turn2.example.com");
    REQUIRE(profile.ice_credentials.turn_server_list()[1].username == "user2");

    SECTION("all turn servers are kept; JuiceAgent clamps to what libjuice can use") {
        const auto all_three = decoded<ClientProfile>(sw::client_profile_three_turns);
        REQUIRE(all_three.ice_credentials.turn_server_list().size() == 3);
        REQUIRE(all_three.ice_credentials.turn_server_list()[2].host == "turn3.example.com");
    }

    SECTION("a nil turn server list decodes as empty (the server's no-TURN path)") {
        // crowserve's IceDefault fallback leaves TurnServers nil, which
        // fxamacker encodes as CBOR null; this must not fail the ClientProfile
        const auto none = decoded<ClientProfile>(sw::client_profile_no_turn_servers);
        REQUIRE(none.ice_credentials.turn_server_list().empty());
    }

    const auto response = decoded<AdvertisementsResponse>(sw::advertisements_response);
    REQUIRE(response.ad_files.size() == 2);
    REQUIRE(response.ad_files[0] == wire_fixtures::make_ad_file());
    REQUIRE(response.ad_files[1] == AdFile{});

    const auto connection_request = decoded<P2P::ConnectionRequest>(sw::p2p_connection_request);
    REQUIRE(connection_request.header.agent_type == JuiceAgentType::P2P);
    REQUIRE(
        connection_request.header.peer_id == wire_fixtures::make_address(
                                                  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}
                                              )
    );
    REQUIRE(connection_request.counter == 0x0a0b0c0d);

    const auto pong = decoded<Pong>(sw::p2p_pong);
    REQUIRE(pong.header.agent_type == JuiceAgentType::RelayFallback);
    REQUIRE(pong.header.peer_id == wire_fixtures::make_filled_address(0xaa));
    REQUIRE(pong.timestamp == "2026-09-27T00:00:00Z");

    const auto description = decoded<JuiceLocalDescription>(sw::p2p_juice_local_description);
    REQUIRE(description.header.agent_type == JuiceAgentType::Relay);
    REQUIRE(description.sdp == "v=0\r\no=juice 123 1 IN IP4 192.168.1.4\r\ns=-\r\nt=0 0\r\n");

    const auto candidate = decoded<JuiceCandidate>(sw::p2p_juice_candidate);
    REQUIRE(candidate.header.agent_type == JuiceAgentType::P2P);
    REQUIRE(candidate.header.peer_id == wire_fixtures::make_filled_address(0x55));
    REQUIRE(candidate.candidate == "candidate:842163049 1 udp 1677729535 192.168.1.4 61234 typ srflx");

    const auto done = decoded<JuiceDone>(sw::p2p_juice_done);
    REQUIRE(done.header.agent_type == JuiceAgentType::Relay);
    REQUIRE(done.header.peer_id == wire_fixtures::make_filled_address(0x77));
}

TEST_CASE("Malformed and legacy wire shapes pin current behavior") {
    using namespace CrownLinkProtocol;
    namespace mw = wire_fixtures::malformed_wire;

    SECTION("unknown fields are ignored on decode") {
        const auto request = decoded<ConnectionRequest>(mw::connection_request_unknown_field);
        REQUIRE(request.lobby_password == "hunter2");
        REQUIRE(request.crownlink_version == 0x12345678);
    }

    SECTION("missing required fields report failure") {
        // decode fails cleanly and the caller skips the handler (the old
        // nlohmann path escaped this as an uncaught json::out_of_range)
        REQUIRE(decode_fails<ConnectionRequest>(mw::connection_request_missing_lobby_password));
    }

    SECTION("wrong-typed fields report failure") {
        REQUIRE(decode_fails<ConnectionRequest>(mw::connection_request_wrong_type_password));
    }

    SECTION("short NetAddress bytes are copied partially (lenient, unlike crowserve)") {
        // the server's NetId decoder rejects anything but 16 bytes and drops
        // the connection; the C++ side accepts what is available
        const auto request = decoded<ConnectionRequest>(mw::connection_request_short_address);
        REQUIRE(request.requested_id == wire_fixtures::make_address({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}));
    }

    SECTION("legacy P2P headers without agent_type report failure") {
        // older CrownLink clients omitted agent_type; the server tolerates the
        // missing field (decodes it as RelayFallback) and re-encodes every
        // relayed message with agent_type present, so this shape cannot reach
        // the client decoder in production - decoding it cleanly fails.
        REQUIRE(decode_fails<P2P::ConnectionRequest>(mw::p2p_legacy_header_missing_agent_type));
    }
}

TEST_CASE("Deserialization failure handling") {
    namespace rw = wire_fixtures::reflected_wire;

    SECTION("garbage bytes report failure") {
        auto bytes = std::vector<u8>{0xff, 0xff};
        auto request = CrownLinkProtocol::ConnectionRequest{};
        REQUIRE(!deserialize_cbor_into(request, std::span<u8>(bytes)));
    }

    SECTION("empty input reports failure") {
        auto bytes = std::vector<u8>{};
        auto request = CrownLinkProtocol::ConnectionRequest{};
        REQUIRE(!deserialize_cbor_into(request, std::span<u8>(bytes)));
    }

    SECTION("truncated message reports failure") {
        auto bytes = wire_fixtures::from_hex(rw::connection_request_full);
        bytes.pop_back();
        auto request = CrownLinkProtocol::ConnectionRequest{};
        REQUIRE(!deserialize_cbor_into(request, std::span<u8>(bytes)));
    }

    SECTION("trailing bytes after the message are tolerated") {
        // jsoncons parses leniently; the old nlohmann reader rejected trailing
        // bytes. Message framing bounds each buffer to exactly one item, so
        // this cannot occur on real traffic.
        auto bytes = wire_fixtures::from_hex(rw::connection_request_full);
        bytes.push_back(0x01);
        auto request = CrownLinkProtocol::ConnectionRequest{};
        REQUIRE(deserialize_cbor_into(request, std::span<u8>(bytes)));
    }
}