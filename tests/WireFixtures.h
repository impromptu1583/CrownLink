#pragma once

// Golden-byte fixtures pinning the CBOR wire contract between CrownLink.dll and
// crowserve. The make_* functions build fixture values; the hex constants in
// the namespaces below are generated (do not hand-edit them):
//   - reflected_wire: bytes the current serializer (reflect-cpp via
//     tests/wire_oracle.cpp) emits; crowserve's tests prove the server decodes
//     them, and the golden serialize tests below pin them
//   - crownlink_wire / malformed_wire: frozen record of what the previous
//     nlohmann serializer emitted - kept as decode-regression inputs so old
//     shapes stay readable; verified byte-for-byte by
//     tests/wire_fixture_generator.py's nlohmann-rules encoder
//   - crowserve_wire: bytes crowserve's CborEnc (fxamacker/cbor) emits for
//     server->client and relayed messages, captured by its
//     wire_fixture_writer_test.go
// The same bytes are mirrored in crowserve's wire_fixtures_test.go files.
//
// These constants are a cross-language compatibility contract. If a test here
// fails after a serializer change, the serializer is wrong, not the test.
// Deliberate wire changes must update both sides at once (rerun the oracle and
// the generator; see wire_fixture_generator.py's header).

#include "crow_serve/CrownLinkProtocol.h"
#include "crow_serve/P2PProtocol.h"
#include "shared/StormTypes.h"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wire_fixtures {

inline std::string to_hex(std::span<const u8> bytes) {
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        out += std::format("{:02x}", byte);
    }
    return out;
}

inline int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

inline std::vector<u8> from_hex(std::string_view hex) {
    std::vector<u8> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t index = 0; index + 1 < hex.size(); index += 2) {
        const auto high = hex_digit(hex[index]);
        const auto low = hex_digit(hex[index + 1]);
        bytes.push_back(static_cast<u8>(high * 16 + low));
    }
    return bytes;
}

inline NetAddress make_address(std::initializer_list<u8> bytes) {
    NetAddress address{};
    std::ranges::copy(bytes, address.bytes.begin());
    return address;
}

inline NetAddress make_filled_address(u8 fill_byte) {
    NetAddress address{};
    address.bytes.fill(fill_byte);
    return address;
}

inline GameInfo make_game_info() {
    GameInfo game{};
    game.game_index = 1;
    game.game_state = 2;
    game.creation_time = 3;
    game.host = make_address(
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}
    );
    game.host_latency = 4;
    game.host_last_time = 5;
    game.category_bits = 6;
    // latin1 bytes: "t<EB>st n<E4>me" / "d<E9>sc" exercise the simdutf conversion on the wire
    const auto game_name = std::array{'t', '\xeb', 's', 't', ' ', 'n', '\xe4', 'm', 'e'};
    const auto game_description = std::array{'d', '\xeb', 's', 'c'};
    std::ranges::copy(game_name, game.game_name);
    std::ranges::copy(game_description, game.game_description);
    game.extra_bytes = 32;
    game.program_id = 7;
    game.version_id = 8;
    return game;
}

inline AdFile make_ad_file() {
    AdFile ad{};
    ad.game_info = make_game_info();
    for (u8 index = 0; auto& byte : ad.extra_bytes) {
        byte = static_cast<char>(index++);
    }
    ad.turns_per_second = std::to_underlying(TurnsPerSecond::Standard);
    return ad;
}

inline CrownLinkProtocol::ConnectionRequest make_connection_request_full() {
    auto request = CrownLinkProtocol::ConnectionRequest{};
    request.preshared_key = "preshared";
    request.peer_id_requested = true;
    request.requested_id = make_address(
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}
    );
    request.request_token = make_address(
        {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff}
    );
    request.lobby_password = "hunter2";
    request.product_id = 1;
    request.version_id = 300;
    request.crownlink_version = 0x12345678;
    return request;
}

inline CrownLinkProtocol::ConnectionRequest make_connection_request_minimal() {
    auto request = CrownLinkProtocol::ConnectionRequest{};
    request.preshared_key = "";
    request.peer_id_requested = false;
    request.requested_id = make_address({});
    request.request_token = make_address({});
    request.lobby_password = "";
    request.product_id = 0;
    request.version_id = 0;
    request.crownlink_version = 0;
    return request;
}

inline CrownLinkProtocol::KeyExchange make_key_exchange() {
    auto key_exchange = CrownLinkProtocol::KeyExchange{};
    key_exchange.public_key = "MCowBQYDK2VwAyEAG8f2Xy0PzL8vQw3R";
    return key_exchange;
}

inline CrownLinkProtocol::TurnServer make_turn_server() {
    auto turn_server = CrownLinkProtocol::TurnServer{};
    turn_server.host = "turn.example.com";
    turn_server.port = "3478";
    turn_server.username = "user";
    turn_server.password = "pass";
    return turn_server;
}

inline CrownLinkProtocol::IceCredentials make_ice_credentials() {
    auto ice_credentials = CrownLinkProtocol::IceCredentials{};
    ice_credentials.stun_host = "stun.example.com";
    ice_credentials.stun_port = "3478";
    ice_credentials.turn_servers = std::vector<CrownLinkProtocol::TurnServer>{};
    return ice_credentials;
}

inline CrownLinkProtocol::ClientProfile make_client_profile() {
    auto client_profile = CrownLinkProtocol::ClientProfile{};
    client_profile.peer_id = make_address(
        {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}
    );
    client_profile.token = make_address(
        {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f}
    );
    client_profile.ice_credentials = make_ice_credentials();
    return client_profile;
}

inline CrownLinkProtocol::UpdateAvailable make_update_available() {
    auto update_available = CrownLinkProtocol::UpdateAvailable{};
    update_available.min_clink_version = 42;
    return update_available;
}

inline CrownLinkProtocol::StartAdvertising make_start_advertising() {
    auto message = CrownLinkProtocol::StartAdvertising{};
    message.ad = make_ad_file();
    message.lobby_password = "pw";
    return message;
}

inline CrownLinkProtocol::StopAdvertising make_stop_advertising() {
    return CrownLinkProtocol::StopAdvertising{};
}

inline CrownLinkProtocol::AdvertisementsRequest make_advertisements_request() {
    auto message = CrownLinkProtocol::AdvertisementsRequest{};
    message.lobby_password = "";
    return message;
}

inline CrownLinkProtocol::AdvertisementsResponse make_advertisements_response() {
    auto message = CrownLinkProtocol::AdvertisementsResponse{};
    message.ad_files = {make_ad_file(), AdFile{}};
    return message;
}

inline CrownLinkProtocol::EchoRequest make_echo_request() {
    auto message = CrownLinkProtocol::EchoRequest{};
    message.message = "ping";
    return message;
}

inline CrownLinkProtocol::EchoResponse make_echo_response() {
    auto message = CrownLinkProtocol::EchoResponse{};
    message.message = "pong";
    return message;
}

inline P2P::Header make_p2p_header(const NetAddress& peer_id, JuiceAgentType agent_type) {
    return P2P::Header{peer_id, agent_type};
}

inline P2P::ConnectionRequest make_p2p_connection_request() {
    return P2P::ConnectionRequest{
        make_p2p_header(
            make_address({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f}),
            JuiceAgentType::P2P
        ),
        0x0a0b0c0d,
    };
}

inline P2P::Pong make_p2p_pong() {
    return P2P::Pong{make_p2p_header(make_filled_address(0xaa), JuiceAgentType::RelayFallback), "2026-09-27T00:00:00Z"};
}

inline P2P::JuiceLocalDescription make_p2p_juice_local_description() {
    return P2P::JuiceLocalDescription{
        make_p2p_header(
            make_address({0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f}),
            JuiceAgentType::Relay
        ),
        "v=0\r\no=juice 123 1 IN IP4 192.168.1.4\r\ns=-\r\nt=0 0\r\n",
    };
}

inline P2P::JuiceCandidate make_p2p_juice_candidate() {
    return P2P::JuiceCandidate{
        make_p2p_header(make_filled_address(0x55), JuiceAgentType::P2P),
        "candidate:842163049 1 udp 1677729535 192.168.1.4 61234 typ srflx",
    };
}

inline P2P::JuiceDone make_p2p_juice_done() {
    return P2P::JuiceDone{make_p2p_header(make_filled_address(0x77), JuiceAgentType::Relay)};
}

// ---------------------------------------------------------------------------
// Golden bytes. Captured by tests/wire_oracle.cpp (nlohmann) and by
// crowserve's CborEnc (fxamacker); do not hand-edit.
// ---------------------------------------------------------------------------

namespace crownlink_wire {
// [wire-fixture-generator:begin:crownlink_wire]
    inline constexpr std::string_view connection_request_full =
    "a87163726f776e6c696e6b5f76657273696f6e1a123456786e6c6f6262795f70"
    "617373776f72646768756e7465723271706565725f69645f7265717565737465"
    "64f56d7072657368617265645f6b6579697072657368617265646a70726f6475"
    "63745f6964016d726571756573745f746f6b656e50f0f1f2f3f4f5f6f7f8f9fa"
    "fbfcfdfeff6c7265717565737465645f696450000102030405060708090a0b0c"
    "0d0e0f6a76657273696f6e5f696419012c";
    inline constexpr std::string_view connection_request_minimal =
    "a87163726f776e6c696e6b5f76657273696f6e006e6c6f6262795f7061737377"
    "6f72646071706565725f69645f726571756573746564f46d7072657368617265"
    "645f6b6579606a70726f647563745f6964006d726571756573745f746f6b656e"
    "50000000000000000000000000000000006c7265717565737465645f69645000"
    "0000000000000000000000000000006a76657273696f6e5f696400";
    inline constexpr std::string_view key_exchange =
    "a16a7075626c69635f6b657978204d436f77425159444b325677417945414738"
    "6632587930507a4c387651773352";
    inline constexpr std::string_view turn_server =
    "a464686f7374707475726e2e6578616d706c652e636f6d6870617373776f7264"
    "647061737364706f7274643334373868757365726e616d656475736572";
    inline constexpr std::string_view ice_credentials =
    "a2697374756e5f686f7374707374756e2e6578616d706c652e636f6d69737475"
    "6e5f706f72746433343738";
    inline constexpr std::string_view client_profile =
    "a36f6963655f63726564656e7469616c73a2697374756e5f686f737470737475"
    "6e2e6578616d706c652e636f6d697374756e5f706f7274643334373867706565"
    "725f696450101112131415161718191a1b1c1d1e1f65746f6b656e5020212223"
    "2425262728292a2b2c2d2e2f";
    inline constexpr std::string_view update_available =
    "a1716d696e5f636c696e6b5f76657273696f6e182a";
    inline constexpr std::string_view start_advertising =
    "a2626164a36e63726f776e6c696e6b5f6d6f6465086b65787472615f62797465"
    "735820000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c"
    "1d1e1f6967616d655f696e666fac6d63617465676f72795f62697473066d6372"
    "656174696f6e5f74696d65036b65787472615f627974657318207067616d655f"
    "6465736372697074696f6e6564c3ab73636a67616d655f696e64657801696761"
    "6d655f6e616d656b74c3ab7374206ec3a46d656a67616d655f73746174650264"
    "686f737450000102030405060708090a0b0c0d0e0f6e686f73745f6c6173745f"
    "74696d65056c686f73745f6c6174656e6379046a70726f6772616d5f6964076a"
    "76657273696f6e5f6964086e6c6f6262795f70617373776f7264627077";
    inline constexpr std::string_view stop_advertising =
    "f6";
    inline constexpr std::string_view advertisements_request =
    "a16e6c6f6262795f70617373776f726460";
    inline constexpr std::string_view advertisements_response =
    "a16861645f66696c657382a36e63726f776e6c696e6b5f6d6f6465086b657874"
    "72615f62797465735820000102030405060708090a0b0c0d0e0f101112131415"
    "161718191a1b1c1d1e1f6967616d655f696e666fac6d63617465676f72795f62"
    "697473066d6372656174696f6e5f74696d65036b65787472615f627974657318"
    "207067616d655f6465736372697074696f6e6564c3ab73636a67616d655f696e"
    "646578016967616d655f6e616d656b74c3ab7374206ec3a46d656a67616d655f"
    "73746174650264686f737450000102030405060708090a0b0c0d0e0f6e686f73"
    "745f6c6173745f74696d65056c686f73745f6c6174656e6379046a70726f6772"
    "616d5f6964076a76657273696f6e5f696408a36e63726f776e6c696e6b5f6d6f"
    "6465006b65787472615f62797465735820000000000000000000000000000000"
    "00000000000000000000000000000000006967616d655f696e666fac6d636174"
    "65676f72795f62697473006d6372656174696f6e5f74696d65006b6578747261"
    "5f6279746573007067616d655f6465736372697074696f6e606a67616d655f69"
    "6e646578006967616d655f6e616d65606a67616d655f73746174650064686f73"
    "7450000000000000000000000000000000006e686f73745f6c6173745f74696d"
    "65006c686f73745f6c6174656e6379006a70726f6772616d5f6964006a766572"
    "73696f6e5f696400";
    inline constexpr std::string_view echo_request =
    "a1676d6573736167656470696e67";
    inline constexpr std::string_view echo_response =
    "a1676d65737361676564706f6e67";
    inline constexpr std::string_view p2p_connection_request =
    "a267636f756e7465721a0a0b0c0d66686561646572a26a6167656e745f747970"
    "650167706565725f696450000102030405060708090a0b0c0d0e0f";
    inline constexpr std::string_view p2p_pong =
    "a266686561646572a26a6167656e745f747970650067706565725f696450aaaa"
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaa6974696d657374616d7074323032362d3039"
    "2d32375430303a30303a30305a";
    inline constexpr std::string_view p2p_juice_local_description =
    "a266686561646572a26a6167656e745f747970650267706565725f6964501011"
    "12131415161718191a1b1c1d1e1f637364707833763d300d0a6f3d6a75696365"
    "20313233203120494e20495034203139322e3136382e312e340d0a733d2d0d0a"
    "743d3020300d0a";
    inline constexpr std::string_view p2p_juice_candidate =
    "a26963616e646964617465784063616e6469646174653a383432313633303439"
    "2031207564702031363737373239353335203139322e3136382e312e34203631"
    "32333420747970207372666c7866686561646572a26a6167656e745f74797065"
    "0167706565725f69645055555555555555555555555555555555";
    inline constexpr std::string_view p2p_juice_done =
    "a166686561646572a26a6167656e745f747970650267706565725f6964507777"
    "7777777777777777777777777777";
// [wire-fixture-generator:end]
}

namespace reflected_wire {
// [wire-fixture-generator:begin:reflected_wire]
    inline constexpr std::string_view connection_request_full =
    "bf6d7072657368617265645f6b65796970726573686172656471706565725f69"
    "645f726571756573746564f56c7265717565737465645f696450000102030405"
    "060708090a0b0c0d0e0f6d726571756573745f746f6b656e50f0f1f2f3f4f5f6"
    "f7f8f9fafbfcfdfeff6e6c6f6262795f70617373776f72646768756e74657232"
    "6a70726f647563745f6964016a76657273696f6e5f696419012c7163726f776e"
    "6c696e6b5f76657273696f6e1a12345678ff";
    inline constexpr std::string_view connection_request_minimal =
    "bf6d7072657368617265645f6b65796071706565725f69645f72657175657374"
    "6564f46c7265717565737465645f696450000000000000000000000000000000"
    "006d726571756573745f746f6b656e5000000000000000000000000000000000"
    "6e6c6f6262795f70617373776f7264606a70726f647563745f6964006a766572"
    "73696f6e5f6964007163726f776e6c696e6b5f76657273696f6e00ff";
    inline constexpr std::string_view key_exchange =
    "bf6a7075626c69635f6b657978204d436f77425159444b325677417945414738"
    "6632587930507a4c387651773352ff";
    inline constexpr std::string_view turn_server =
    "bf64686f7374707475726e2e6578616d706c652e636f6d64706f727464333437"
    "3868757365726e616d6564757365726870617373776f72646470617373ff";
    inline constexpr std::string_view ice_credentials =
    "bf697374756e5f686f7374707374756e2e6578616d706c652e636f6d69737475"
    "6e5f706f727464333437386c7475726e5f7365727665727380ff";
    inline constexpr std::string_view client_profile =
    "bf67706565725f696450101112131415161718191a1b1c1d1e1f65746f6b656e"
    "50202122232425262728292a2b2c2d2e2f6f6963655f63726564656e7469616c"
    "73bf697374756e5f686f7374707374756e2e6578616d706c652e636f6d697374"
    "756e5f706f727464333437386c7475726e5f7365727665727380ffff";
    inline constexpr std::string_view update_available =
    "bf716d696e5f636c696e6b5f76657273696f6e182aff";
    inline constexpr std::string_view start_advertising =
    "bf626164bf6967616d655f696e666fbf6a67616d655f696e646578016a67616d"
    "655f7374617465026d6372656174696f6e5f74696d650364686f737450000102"
    "030405060708090a0b0c0d0e0f6c686f73745f6c6174656e6379046e686f7374"
    "5f6c6173745f74696d65056d63617465676f72795f62697473066967616d655f"
    "6e616d656b74c3ab7374206ec3a46d657067616d655f6465736372697074696f"
    "6e6564c3ab73636b65787472615f627974657318206a70726f6772616d5f6964"
    "076a76657273696f6e5f696408ff6b65787472615f6279746573582000010203"
    "0405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f6e63726f"
    "776e6c696e6b5f6d6f646508ff6e6c6f6262795f70617373776f7264627077ff";
    inline constexpr std::string_view stop_advertising =
    "bfff";
    inline constexpr std::string_view advertisements_request =
    "bf6e6c6f6262795f70617373776f726460ff";
    inline constexpr std::string_view advertisements_response =
    "bf6861645f66696c657382bf6967616d655f696e666fbf6a67616d655f696e64"
    "6578016a67616d655f7374617465026d6372656174696f6e5f74696d65036468"
    "6f737450000102030405060708090a0b0c0d0e0f6c686f73745f6c6174656e63"
    "79046e686f73745f6c6173745f74696d65056d63617465676f72795f62697473"
    "066967616d655f6e616d656b74c3ab7374206ec3a46d657067616d655f646573"
    "6372697074696f6e6564c3ab73636b65787472615f627974657318206a70726f"
    "6772616d5f6964076a76657273696f6e5f696408ff6b65787472615f62797465"
    "735820000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c"
    "1d1e1f6e63726f776e6c696e6b5f6d6f646508ffbf6967616d655f696e666fbf"
    "6a67616d655f696e646578006a67616d655f7374617465006d6372656174696f"
    "6e5f74696d650064686f737450000000000000000000000000000000006c686f"
    "73745f6c6174656e6379006e686f73745f6c6173745f74696d65006d63617465"
    "676f72795f62697473006967616d655f6e616d65607067616d655f6465736372"
    "697074696f6e606b65787472615f6279746573006a70726f6772616d5f696400"
    "6a76657273696f6e5f696400ff6b65787472615f627974657358200000000000"
    "0000000000000000000000000000000000000000000000000000006e63726f77"
    "6e6c696e6b5f6d6f646500ffff";
    inline constexpr std::string_view echo_request =
    "bf676d6573736167656470696e67ff";
    inline constexpr std::string_view echo_response =
    "bf676d65737361676564706f6e67ff";
    inline constexpr std::string_view p2p_connection_request =
    "bf66686561646572bf67706565725f696450000102030405060708090a0b0c0d"
    "0e0f6a6167656e745f7479706501ff67636f756e7465721a0a0b0c0dff";
    inline constexpr std::string_view p2p_pong =
    "bf66686561646572bf67706565725f696450aaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaa6a6167656e745f7479706500ff6974696d657374616d7074323032362d30"
    "392d32375430303a30303a30305aff";
    inline constexpr std::string_view p2p_juice_local_description =
    "bf66686561646572bf67706565725f696450101112131415161718191a1b1c1d"
    "1e1f6a6167656e745f7479706502ff637364707833763d300d0a6f3d6a756963"
    "6520313233203120494e20495034203139322e3136382e312e340d0a733d2d0d"
    "0a743d3020300d0aff";
    inline constexpr std::string_view p2p_juice_candidate =
    "bf66686561646572bf67706565725f6964505555555555555555555555555555"
    "55556a6167656e745f7479706501ff6963616e646964617465784063616e6469"
    "646174653a383432313633303439203120756470203136373737323935333520"
    "3139322e3136382e312e3420363132333420747970207372666c78ff";
    inline constexpr std::string_view p2p_juice_done =
    "bf66686561646572bf67706565725f6964507777777777777777777777777777"
    "77776a6167656e745f7479706502ffff";
// [wire-fixture-generator:end]
}

namespace crowserve_wire {
// [wire-fixture-generator:begin:crowserve_wire]
    inline constexpr std::string_view client_profile =
    "a365746f6b656e50202122232425262728292a2b2c2d2e2f67706565725f6964"
    "50101112131415161718191a1b1c1d1e1f6f6963655f63726564656e7469616c"
    "73a3697374756e5f686f7374707374756e2e6578616d706c652e636f6d697374"
    "756e5f706f727464333437386c7475726e5f7365727665727382a464686f7374"
    "707475726e2e6578616d706c652e636f6d64706f727464333437386870617373"
    "776f7264647061737368757365726e616d656475736572a464686f7374717475"
    "726e322e6578616d706c652e636f6d64706f727464333437396870617373776f"
    "726465706173733268757365726e616d65657573657232";
    inline constexpr std::string_view client_profile_three_turns =
    "a365746f6b656e50202122232425262728292a2b2c2d2e2f67706565725f6964"
    "50101112131415161718191a1b1c1d1e1f6f6963655f63726564656e7469616c"
    "73a3697374756e5f686f7374707374756e2e6578616d706c652e636f6d697374"
    "756e5f706f727464333437386c7475726e5f7365727665727383a464686f7374"
    "707475726e2e6578616d706c652e636f6d64706f727464333437386870617373"
    "776f7264647061737368757365726e616d656475736572a464686f7374717475"
    "726e322e6578616d706c652e636f6d64706f727464333437396870617373776f"
    "726465706173733268757365726e616d65657573657232a464686f7374717475"
    "726e332e6578616d706c652e636f6d64706f727464333438306870617373776f"
    "726465706173733368757365726e616d65657573657233";
    inline constexpr std::string_view client_profile_no_turn_servers =
    "a365746f6b656e50202122232425262728292a2b2c2d2e2f67706565725f6964"
    "50101112131415161718191a1b1c1d1e1f6f6963655f63726564656e7469616c"
    "73a3697374756e5f686f7374707374756e2e6578616d706c652e636f6d697374"
    "756e5f706f727464333437386c7475726e5f73657276657273f6";
    inline constexpr std::string_view advertisements_response =
    "a16861645f66696c657382a36967616d655f696e666fac64686f737450000102"
    "030405060708090a0b0c0d0e0f6967616d655f6e616d656b74c3ab7374206ec3"
    "a46d656a67616d655f696e646578016a67616d655f7374617465026a70726f67"
    "72616d5f6964076a76657273696f6e5f6964086b65787472615f627974657318"
    "206c686f73745f6c6174656e6379046d63617465676f72795f62697473066d63"
    "72656174696f6e5f74696d65036e686f73745f6c6173745f74696d6505706761"
    "6d655f6465736372697074696f6e6564c3ab73636b65787472615f6279746573"
    "5820000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d"
    "1e1f6e63726f776e6c696e6b5f6d6f646508a36967616d655f696e666fac6468"
    "6f737450000000000000000000000000000000006967616d655f6e616d65606a"
    "67616d655f696e646578006a67616d655f7374617465006a70726f6772616d5f"
    "6964006a76657273696f6e5f6964006b65787472615f6279746573006c686f73"
    "745f6c6174656e6379006d63617465676f72795f62697473006d637265617469"
    "6f6e5f74696d65006e686f73745f6c6173745f74696d65007067616d655f6465"
    "736372697074696f6e606b65787472615f627974657358200000000000000000"
    "0000000000000000000000000000000000000000000000006e63726f776e6c69"
    "6e6b5f6d6f646500";
    inline constexpr std::string_view p2p_connection_request =
    "a266686561646572a267706565725f696450000102030405060708090a0b0c0d"
    "0e0f6a6167656e745f747970650167636f756e7465721a0a0b0c0d";
    inline constexpr std::string_view p2p_pong =
    "a266686561646572a267706565725f696450aaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    "aaaa6a6167656e745f74797065006974696d657374616d7074323032362d3039"
    "2d32375430303a30303a30305a";
    inline constexpr std::string_view p2p_juice_local_description =
    "a2637364707833763d300d0a6f3d6a7569636520313233203120494e20495034"
    "203139322e3136382e312e340d0a733d2d0d0a743d3020300d0a666865616465"
    "72a267706565725f696450101112131415161718191a1b1c1d1e1f6a6167656e"
    "745f7479706502";
    inline constexpr std::string_view p2p_juice_candidate =
    "a266686561646572a267706565725f6964505555555555555555555555555555"
    "55556a6167656e745f74797065016963616e646964617465784063616e646964"
    "6174653a38343231363330343920312075647020313637373732393533352031"
    "39322e3136382e312e3420363132333420747970207372666c78";
    inline constexpr std::string_view p2p_juice_done =
    "a166686561646572a267706565725f6964507777777777777777777777777777"
    "77776a6167656e745f7479706502";
// [wire-fixture-generator:end]
}

namespace malformed_wire {
// [wire-fixture-generator:begin:malformed_wire]
    inline constexpr std::string_view connection_request_missing_lobby_password =
    "a77163726f776e6c696e6b5f76657273696f6e1a1234567871706565725f6964"
    "5f726571756573746564f56d7072657368617265645f6b657969707265736861"
    "7265646a70726f647563745f6964016d726571756573745f746f6b656e50f0f1"
    "f2f3f4f5f6f7f8f9fafbfcfdfeff6c7265717565737465645f69645000010203"
    "0405060708090a0b0c0d0e0f6a76657273696f6e5f696419012c";
    inline constexpr std::string_view connection_request_wrong_type_password =
    "a87163726f776e6c696e6b5f76657273696f6e1a123456786e6c6f6262795f70"
    "617373776f72640571706565725f69645f726571756573746564f56d70726573"
    "68617265645f6b6579697072657368617265646a70726f647563745f6964016d"
    "726571756573745f746f6b656e50f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff6c72"
    "65717565737465645f696450000102030405060708090a0b0c0d0e0f6a766572"
    "73696f6e5f696419012c";
    inline constexpr std::string_view connection_request_unknown_field =
    "a97163726f776e6c696e6b5f76657273696f6e1a123456786c6675747572655f"
    "6669656c64182a6e6c6f6262795f70617373776f72646768756e746572327170"
    "6565725f69645f726571756573746564f56d7072657368617265645f6b657969"
    "7072657368617265646a70726f647563745f6964016d726571756573745f746f"
    "6b656e50f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff6c7265717565737465645f69"
    "6450000102030405060708090a0b0c0d0e0f6a76657273696f6e5f696419012c";
    inline constexpr std::string_view connection_request_short_address =
    "a87163726f776e6c696e6b5f76657273696f6e1a123456786e6c6f6262795f70"
    "617373776f72646768756e7465723271706565725f69645f7265717565737465"
    "64f56d7072657368617265645f6b6579697072657368617265646a70726f6475"
    "63745f6964016d726571756573745f746f6b656e50f0f1f2f3f4f5f6f7f8f9fa"
    "fbfcfdfeff6c7265717565737465645f69644800010203040506076a76657273"
    "696f6e5f696419012c";
    inline constexpr std::string_view p2p_legacy_header_missing_agent_type =
    "a267636f756e7465721a0a0b0c0d66686561646572a167706565725f69645000"
    "0102030405060708090a0b0c0d0e0f";
// [wire-fixture-generator:end]
}

}  // namespace wire_fixtures