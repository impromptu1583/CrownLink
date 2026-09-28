// One-shot generator for the golden-byte constants in WireFixtures.h.
//
// Not part of the CMake build. It compiles the REAL protocol headers and
// serialize_cbor implementation (reflect-cpp v0.25.0) and writes the exact
// bytes each fixture produces to a machine-readable file (name TAB hex), so
// the constants in WireFixtures.h and crowserve's fixture tests are generated
// without hand-copying hex. tests/wire_fixture_generator.py verifies these
// bytes against an independent Python encoder and splices them into the test
// sources.
//
// The constants produced today land in the `reflected_wire` namespace: bytes
// the current reflect-cpp serializer emits, which the crowserve (fxamacker)
// tests prove the server decodes. The `crownlink_wire` and `malformed_wire`
// constants are FROZEN records of what the previous nlohmann serializer
// emitted - clients must keep decoding those shapes - and are verified by the
// generator's nlohmann-rules encoder, not by this oracle.
//
// Build (from the CrownLink repo root; needs a C++23 compiler, the deps
// fetched by CMake under out/build/x86-release/_deps, and the shim in
// tests/wire_oracle_shim/ which replaces simdutf with a byte-equivalent
// scalar conversion):
//
//   cp -r out/build/x86-release/_deps/storm_sdk-src/include /tmp/storm_include
//   sed -i 's/static_assert(sizeof(NetFunctions) == 112);//' /tmp/storm_include/storm/util/snet.hpp
//   RFL=...reflect-cpp-src; JC=...jsoncons-src
//   g++ -std=c++23 -D__stdcall= -D__fastcall= \
//       $RFL/src/reflectcpp.cpp $RFL/src/reflectcpp_cbor.cpp tests/wire_oracle.cpp -o /tmp/oracle \
//       -Itests/wire_oracle_shim -Isrc -Iinclude -I/tmp/storm_include \
//       -Iout/build/x86-release/generated -I$RFL/include -I$JC/include -I$RFL/include/rfl/thirdparty
//   /tmp/oracle wire_fixtures_crownlink.txt
//
// Then run crowserve's fixture writer and tests/wire_fixture_generator.py
// (see the generator's header for the full flow).

#include "WireFixtures.h"
#include "shared/Cbor.h"

#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string hex_of(const auto& message) {
    std::vector<u8> bytes{};
    serialize_cbor(message, bytes);
    return wire_fixtures::to_hex(bytes);
}

}  // namespace

int main(int argc, char* argv[]) {
    const char* output_path = argc > 1 ? argv[1] : "wire_fixtures_crownlink.txt";
    std::ofstream out{output_path};

    const auto emit = [&](std::string_view name, std::string_view hex) {
        out << name << '\t' << hex << '\n';
    };
    const auto emit_message = [&](std::string_view name, const auto& message) {
        emit(name, hex_of(message));
    };

    emit_message("connection_request_full", wire_fixtures::make_connection_request_full());
    emit_message("connection_request_minimal", wire_fixtures::make_connection_request_minimal());
    emit_message("key_exchange", wire_fixtures::make_key_exchange());
    emit_message("turn_server", wire_fixtures::make_turn_server());
    emit_message("ice_credentials", wire_fixtures::make_ice_credentials());
    emit_message("client_profile", wire_fixtures::make_client_profile());
    emit_message("update_available", wire_fixtures::make_update_available());
    emit_message("start_advertising", wire_fixtures::make_start_advertising());
    emit_message("stop_advertising", wire_fixtures::make_stop_advertising());
    emit_message("advertisements_request", wire_fixtures::make_advertisements_request());
    emit_message("advertisements_response", wire_fixtures::make_advertisements_response());
    emit_message("echo_request", wire_fixtures::make_echo_request());
    emit_message("echo_response", wire_fixtures::make_echo_response());
    emit_message("p2p_connection_request", wire_fixtures::make_p2p_connection_request());
    emit_message("p2p_pong", wire_fixtures::make_p2p_pong());
    emit_message("p2p_juice_local_description", wire_fixtures::make_p2p_juice_local_description());
    emit_message("p2p_juice_candidate", wire_fixtures::make_p2p_juice_candidate());
    emit_message("p2p_juice_done", wire_fixtures::make_p2p_juice_done());

    std::printf("wrote %d fixtures to %s\n", 18, output_path);
    return 0;
}