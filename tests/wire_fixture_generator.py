#!/usr/bin/env python3
"""Wire fixture generator: verifies golden bytes and splices them into the test sources.

The CBOR wire contract between CrownLink.dll and crowserve is pinned by golden-byte
fixtures in three places (CrownLink's tests/WireFixtures.h, crowserve's
internal/protocol/{crownlink,p2p}/wire_fixtures_test.go). To eliminate hand-copied
hex, the bytes flow machine-to-machine:

  1. tests/wire_oracle (C++, the real reflect-cpp serialize_cbor) writes
     wire_fixtures_crownlink.txt: "name<TAB>hex" for every client fixture.
     These bytes land in the `reflected_wire` group.
  2. crowserve's TestWriteWireFixturesFile (Go, the real CborEnc) writes
     wire_fixtures_crowserve.txt for server-produced fixtures
     (WIRE_FIXTURE_GENERATE=1 go test ./internal/protocol/crownlink/ -run TestWriteWireFixturesFile).
     These bytes land in the `crowserve_wire` group.
  3. This script re-encodes every fixture independently with two Python
     encoders and asserts byte equality:
       - nlohmann 3.12.0 rules (minimal ints, definite lengths, big-endian,
         map keys sorted bytewise like std::map) against the frozen
         crownlink_wire / malformed_wire constants - the record of what the
         previous serializer emitted, which clients must keep decoding.
       - reflect-cpp/jsoncons rules (minimal ints, definite strings/byte
         strings/arrays, INDEFINITE-length maps in declaration order) against
         the oracle capture.
  4. It then splices the verified bytes between the [wire-fixture-generator]
     markers in all three test files.

Run from the CrownLink repo root after steps 1-2:
    python3 tests/wire_fixture_generator.py
"""

import re
import sys
from pathlib import Path

CROWNSERVE_TXT = Path("../crowserve/internal/protocol/crownlink/wire_fixtures_crowserve.txt")
CROWNLINK_TXT = Path("wire_fixtures_crownlink.txt")
WIRE_FIXTURES_H = Path("tests/WireFixtures.h")
GO_CROWNLINK_TEST = Path("../crowserve/internal/protocol/crownlink/wire_fixtures_test.go")
GO_P2P_TEST = Path("../crowserve/internal/protocol/p2p/wire_fixtures_test.go")

BEGIN = "[wire-fixture-generator:begin"
END = "[wire-fixture-generator:end]"

TRUE = b"\xf5"
FALSE = b"\xf4"

Sdp = "v=0\r\no=juice 123 1 IN IP4 192.168.1.4\r\ns=-\r\nt=0 0\r\n"
Candidate = "candidate:842163049 1 udp 1677729535 192.168.1.4 61234 typ srflx"


def encode_uint(value: int) -> bytes:
    if value <= 0x17:
        return bytes([value])
    if value <= 0xFF:
        return bytes([0x18, value])
    if value <= 0xFFFF:
        return b"\x19" + value.to_bytes(2, "big")
    if value <= 0xFFFFFFFF:
        return b"\x1a" + value.to_bytes(4, "big")
    return b"\x1b" + value.to_bytes(8, "big")


def encode_text(text: str) -> bytes:
    raw = text.encode("utf-8")
    if len(raw) <= 0x17:
        return bytes([0x60 + len(raw)]) + raw
    if len(raw) <= 0xFF:
        return bytes([0x78, len(raw)]) + raw
    return b"\x79" + len(raw).to_bytes(2, "big") + raw


def encode_bytes(data: bytes) -> bytes:
    if len(data) <= 0x17:
        return bytes([0x40 + len(data)]) + data
    if len(data) <= 0xFF:
        return bytes([0x58, len(data)]) + data
    return b"\x59" + len(data).to_bytes(2, "big") + data


# ---------------------------------------------------------------------------
# Fixture models. Dicts are maps (insertion order == wire struct declaration
# order), lists are arrays, bytes are pre-encoded leaf values.
# ---------------------------------------------------------------------------


def game_info(zeros: bool = False) -> dict:
    if zeros:
        return {
            "game_index": encode_uint(0),
            "game_state": encode_uint(0),
            "creation_time": encode_uint(0),
            "host": encode_bytes(bytes(16)),
            "host_latency": encode_uint(0),
            "host_last_time": encode_uint(0),
            "category_bits": encode_uint(0),
            "game_name": encode_text(""),
            "game_description": encode_text(""),
            "extra_bytes": encode_uint(0),
            "program_id": encode_uint(0),
            "version_id": encode_uint(0),
        }
    return {
        "game_index": encode_uint(1),
        "game_state": encode_uint(2),
        "creation_time": encode_uint(3),
        "host": encode_bytes(bytes(range(16))),
        "host_latency": encode_uint(4),
        "host_last_time": encode_uint(5),
        "category_bits": encode_uint(6),
        "game_name": encode_text("tëst näme"),
        "game_description": encode_text("dësc"),
        "extra_bytes": encode_uint(32),
        "program_id": encode_uint(7),
        "version_id": encode_uint(8),
    }


def ad_file(zeros: bool = False) -> dict:
    return {
        "game_info": game_info(zeros),
        "extra_bytes": encode_bytes(bytes(32) if zeros else bytes(range(32))),
        "crownlink_mode": encode_uint(0 if zeros else 8),
    }


def connection_request_full() -> dict:
    return {
        "preshared_key": encode_text("preshared"),
        "peer_id_requested": TRUE,
        "requested_id": encode_bytes(bytes(range(16))),
        "request_token": encode_bytes(bytes(range(0xF0, 0x100))),
        "lobby_password": encode_text("hunter2"),
        "product_id": encode_uint(1),
        "version_id": encode_uint(300),
        "crownlink_version": encode_uint(0x12345678),
    }


def connection_request_minimal() -> dict:
    return {
        "preshared_key": encode_text(""),
        "peer_id_requested": FALSE,
        "requested_id": encode_bytes(bytes(16)),
        "request_token": encode_bytes(bytes(16)),
        "lobby_password": encode_text(""),
        "product_id": encode_uint(0),
        "version_id": encode_uint(0),
        "crownlink_version": encode_uint(0),
    }


def p2p_header(peer: bytes, agent_type: int) -> dict:
    return {"peer_id": encode_bytes(peer), "agent_type": encode_uint(agent_type)}


def fixture_models(reflected: bool = False) -> dict:
    """name -> model for every client-produced fixture. Insertion order of the
    top-level keys must match wire struct declaration order (used by the
    reflect-cpp encoder).

    reflected=False reproduces the frozen nlohmann-era shapes; reflected=True
    the current serializer's shapes. The only difference is client_profile:
    the old IceCredentials to_json dropped turn_servers when writing, while
    the current wire struct is a full mirror and always emits the key."""
    return {
        "connection_request_full": connection_request_full(),
        "connection_request_minimal": connection_request_minimal(),
        "key_exchange": {"public_key": encode_text("MCowBQYDK2VwAyEAG8f2Xy0PzL8vQw3R")},
        "turn_server": {
            "host": encode_text("turn.example.com"),
            "port": encode_text("3478"),
            "username": encode_text("user"),
            "password": encode_text("pass"),
        },
        "ice_credentials": {
            "stun_host": encode_text("stun.example.com"),
            "stun_port": encode_text("3478"),
            **({"turn_servers": []} if reflected else {}),
        },
        "client_profile": {
            "peer_id": encode_bytes(bytes(range(0x10, 0x20))),
            "token": encode_bytes(bytes(range(0x20, 0x30))),
            "ice_credentials": {
                "stun_host": encode_text("stun.example.com"),
                "stun_port": encode_text("3478"),
                **({"turn_servers": []} if reflected else {}),
            },
        },
        "update_available": {"min_clink_version": encode_uint(42)},
        "start_advertising": {"ad": ad_file(), "lobby_password": encode_text("pw")},
        "stop_advertising": {},
        "advertisements_request": {"lobby_password": encode_text("")},
        "advertisements_response": {"ad_files": [ad_file(), ad_file(zeros=True)]},
        "echo_request": {"message": encode_text("ping")},
        "echo_response": {"message": encode_text("pong")},
        "p2p_connection_request": {"header": p2p_header(bytes(range(16)), 1), "counter": encode_uint(0x0A0B0C0D)},
        "p2p_pong": {"header": p2p_header(b"\xaa" * 16, 0), "timestamp": encode_text("2026-09-27T00:00:00Z")},
        "p2p_juice_local_description": {"header": p2p_header(bytes(range(0x10, 0x20)), 2), "sdp": encode_text(Sdp)},
        "p2p_juice_candidate": {"header": p2p_header(b"\x55" * 16, 1), "candidate": encode_text(Candidate)},
        "p2p_juice_done": {"header": p2p_header(b"\x77" * 16, 2)},
    }


def malformed_models() -> dict:
    missing = connection_request_full()
    del missing["lobby_password"]

    wrong_type = connection_request_full()
    wrong_type["lobby_password"] = encode_uint(5)

    unknown_field = connection_request_full()
    unknown_field["future_field"] = encode_uint(42)

    short_address = connection_request_full()
    short_address["requested_id"] = encode_bytes(bytes(range(8)))

    return {
        "connection_request_missing_lobby_password": missing,
        "connection_request_wrong_type_password": wrong_type,
        "connection_request_unknown_field": unknown_field,
        "connection_request_short_address": short_address,
        "p2p_legacy_header_missing_agent_type": {
            "header": {"peer_id": encode_bytes(bytes(range(16)))},
            "counter": encode_uint(0x0A0B0C0D),
        },
    }


# ---------------------------------------------------------------------------
# Two encoders: nlohmann rules (frozen constants) and reflect-cpp/jsoncons
# rules (current serializer). Both share the leaf encodings above.
# ---------------------------------------------------------------------------


def encode_nlohmann(value) -> bytes:
    if isinstance(value, dict):
        keys = sorted(value, key=lambda k: k.encode("utf-8"))
        head = bytes([0xA0 + len(keys)]) if len(keys) <= 0x17 else bytes([0xB8, len(keys)])
        return head + b"".join(encode_text(k) + encode_nlohmann(value[k]) for k in keys)
    if isinstance(value, list):
        head = bytes([0x80 + len(value)]) if len(value) <= 0x17 else bytes([0x98, len(value)])
        return head + b"".join(encode_nlohmann(item) for item in value)
    return value


def encode_reflected(value) -> bytes:
    if isinstance(value, dict):
        return b"\xbf" + b"".join(encode_text(k) + encode_reflected(v) for k, v in value.items()) + b"\xff"
    if isinstance(value, list):
        head = bytes([0x80 + len(value)]) if len(value) <= 0x17 else bytes([0x98, len(value)])
        return head + b"".join(encode_reflected(item) for item in value)
    return value


# ---------------------------------------------------------------------------
# File plumbing
# ---------------------------------------------------------------------------


def read_capture(path: Path) -> dict[str, str]:
    out = {}
    for line in path.read_text().splitlines():
        name, _, hexstr = line.partition("\t")
        out[name] = hexstr
    return out


def read_cpp_constants(path: Path, namespace: str) -> dict[str, str]:
    text = path.read_text()
    section = re.search(rf"namespace {namespace} \{{(.*?)\n\}}", text, re.DOTALL).group(1)
    out = {}
    for match in re.finditer(r"std::string_view (\w+) =\n((?:\s+\"[0-9a-f]*\"\n?)+);", section):
        out[match.group(1)] = "".join(re.findall(r'"([0-9a-f]*)"', match.group(2)))
    return out


def splice(path: Path, marker: str, content: str) -> None:
    text = path.read_text()
    begin_marker = f"// [wire-fixture-generator:begin:{marker}]" if marker else f"// {BEGIN}]"
    i = text.index(begin_marker) + len(begin_marker)
    j = text.index(f"// {END}", i)
    path.write_text(text[:i] + "\n" + content + text[j:])


def cpp_constant(name: str, hexstr: str) -> str:
    chunks = [hexstr[i : i + 64] for i in range(0, len(hexstr), 64)]
    body = "\n".join(f'    "{chunk}"' for chunk in chunks)
    return f"    inline constexpr std::string_view {name} =\n{body};\n"


def go_constant(name: str, hexstr: str) -> str:
    return f'\t{name} = "{hexstr}"\n'


def main() -> int:
    models = fixture_models()
    reflected_models = fixture_models(reflected=True)
    malformed = malformed_models()

    print(f"reading {CROWNLINK_TXT}")
    oracle = read_capture(CROWNLINK_TXT)
    print(f"reading {CROWNSERVE_TXT}")
    crowserve = read_capture(CROWNSERVE_TXT)

    failures = 0

    # frozen nlohmann-era constants: regenerate from models and compare
    frozen = read_cpp_constants(WIRE_FIXTURES_H, "crownlink_wire")
    frozen_malformed = read_cpp_constants(WIRE_FIXTURES_H, "malformed_wire")
    # stop_advertising was CBOR null (0xf6) under nlohmann - special case
    nlohmann_bytes = {name: encode_nlohmann(model) for name, model in models.items()}
    nlohmann_bytes["stop_advertising"] = b"\xf6"
    nlohmann_malformed = {name: encode_nlohmann(model) for name, model in malformed.items()}

    for name, expected in {**nlohmann_bytes, **nlohmann_malformed}.items():
        actual = {**frozen, **frozen_malformed}.get(name)
        mine = expected.hex()
        if actual is None:
            print(f"FAIL {name}: missing from WireFixtures.h")
            failures += 1
        elif actual != mine:
            print(f"FAIL {name}: frozen constant does not match nlohmann-rules encoder")
            print(f"  file  : {actual}")
            print(f"  python: {mine}")
            failures += 1
        else:
            print(f"ok   {name} (nlohmann, frozen)")
    if failures:
        print(f"{failures} frozen fixture(s) failed verification; aborting without writing")
        return 1

    # current reflect-cpp bytes: oracle capture vs rfl-rules encoder
    reflected = {name: encode_reflected(model) for name, model in reflected_models.items()}
    for name, expected in reflected.items():
        actual = oracle.get(name)
        mine = expected.hex()
        if actual is None:
            print(f"FAIL {name}: missing from C++ oracle capture")
            failures += 1
        elif actual != mine:
            print(f"FAIL {name}: C++ oracle and rfl-rules encoder disagree")
            print(f"  oracle : {actual}")
            print(f"  python : {mine}")
            failures += 1
        else:
            print(f"ok   {name} (reflect-cpp, {len(mine) // 2} bytes)")
    if failures:
        print(f"{failures} fixture(s) failed verification; aborting without writing")
        return 1

    print("all fixtures verified")

    # WireFixtures.h: four namespaces
    splice(WIRE_FIXTURES_H, "crownlink_wire", "".join(cpp_constant(n, nlohmann_bytes[n].hex()) for n in models))
    splice(WIRE_FIXTURES_H, "malformed_wire", "".join(cpp_constant(n, nlohmann_malformed[n].hex()) for n in malformed))
    splice(WIRE_FIXTURES_H, "reflected_wire", "".join(cpp_constant(n, reflected[n].hex()) for n in models))
    splice(
        WIRE_FIXTURES_H,
        "crowserve_wire",
        "".join(cpp_constant(name.removeprefix("server_"), crowserve[name]) for name in crowserve),
    )
    print(f"wrote {WIRE_FIXTURES_H}")

    # crowserve Go tests: one const block per package
    go_crownlink = (
        "".join(go_constant(GO_CROWNLINK_CLIENT[name], nlohmann_bytes[name].hex()) for name in models if name in GO_CROWNLINK_CLIENT)
        + "".join(go_constant(GO_CROWNLINK_CLIENT[name], nlohmann_malformed[name].hex()) for name in malformed if name in GO_CROWNLINK_CLIENT)
        + "".join(go_constant(GO_CROWNLINK_REFLECTED[name], reflected[name].hex()) for name in models if name in GO_CROWNLINK_REFLECTED)
        + "".join(go_constant(GO_CROWNLINK_SERVER[name], crowserve[name]) for name in crowserve if name in GO_CROWNLINK_SERVER)
    )
    splice(GO_CROWNLINK_TEST, "", go_crownlink)
    print(f"wrote {GO_CROWNLINK_TEST}")

    go_p2p = (
        "".join(go_constant(GO_P2P_CLIENT[name], nlohmann_bytes[name].hex()) for name in models if name in GO_P2P_CLIENT)
        + "".join(go_constant(GO_P2P_CLIENT[name], nlohmann_malformed[name].hex()) for name in malformed if name in GO_P2P_CLIENT)
        + "".join(go_constant(GO_P2P_REFLECTED[name], reflected[name].hex()) for name in models if name in GO_P2P_REFLECTED)
        + "".join(go_constant(GO_P2P_SERVER[name], crowserve[name]) for name in crowserve if name in GO_P2P_SERVER)
    )
    splice(GO_P2P_TEST, "", go_p2p)
    print(f"wrote {GO_P2P_TEST}")
    return 0


GO_CROWNLINK_CLIENT = {
    "connection_request_full": "crownlinkWireConnectionRequestFull",
    "connection_request_minimal": "crownlinkWireConnectionRequestMinimal",
    "key_exchange": "crownlinkWireKeyExchange",
    "turn_server": "crownlinkWireTurnServer",
    "ice_credentials": "crownlinkWireIceCredentials",
    "client_profile": "crownlinkWireClientProfile",
    "update_available": "crownlinkWireUpdateAvailable",
    "start_advertising": "crownlinkWireStartAdvertising",
    "stop_advertising": "crownlinkWireStopAdvertising",
    "advertisements_request": "crownlinkWireAdvertisementsRequest",
    "advertisements_response": "crownlinkWireAdvertisementsResponse",
    "echo_request": "crownlinkWireEchoRequest",
    "echo_response": "crownlinkWireEchoResponse",
    "connection_request_missing_lobby_password": "crownlinkWireConnectionRequestMissingLobbyPassword",
    "connection_request_wrong_type_password": "crownlinkWireConnectionRequestWrongTypePassword",
    "connection_request_unknown_field": "crownlinkWireConnectionRequestUnknownField",
    "connection_request_short_address": "crownlinkWireConnectionRequestShortAddress",
}

GO_CROWNLINK_REFLECTED = {
    "connection_request_full": "reflectedWireConnectionRequestFull",
    "connection_request_minimal": "reflectedWireConnectionRequestMinimal",
    "key_exchange": "reflectedWireKeyExchange",
    "turn_server": "reflectedWireTurnServer",
    "ice_credentials": "reflectedWireIceCredentials",
    "client_profile": "reflectedWireClientProfile",
    "update_available": "reflectedWireUpdateAvailable",
    "start_advertising": "reflectedWireStartAdvertising",
    "stop_advertising": "reflectedWireStopAdvertising",
    "advertisements_request": "reflectedWireAdvertisementsRequest",
    "advertisements_response": "reflectedWireAdvertisementsResponse",
    "echo_request": "reflectedWireEchoRequest",
    "echo_response": "reflectedWireEchoResponse",
}

GO_CROWNLINK_SERVER = {
    "server_client_profile": "crowserveWireClientProfile",
    "server_client_profile_three_turns": "crowserveWireClientProfileThreeTurns",
    "server_client_profile_no_turn_servers": "crowserveWireClientProfileNoTurnServers",
    "server_advertisements_response": "crowserveWireAdvertisementsResponse",
}

GO_P2P_CLIENT = {
    "p2p_connection_request": "crownlinkWireConnectionRequest",
    "p2p_pong": "crownlinkWirePong",
    "p2p_juice_local_description": "crownlinkWireJuiceLocalDescription",
    "p2p_juice_candidate": "crownlinkWireJuiceCandidate",
    "p2p_juice_done": "crownlinkWireJuiceDone",
    "p2p_legacy_header_missing_agent_type": "crownlinkWireLegacyHeaderMissingAgentType",
}

GO_P2P_REFLECTED = {
    "p2p_connection_request": "reflectedWireConnectionRequest",
    "p2p_pong": "reflectedWirePong",
    "p2p_juice_local_description": "reflectedWireJuiceLocalDescription",
    "p2p_juice_candidate": "reflectedWireJuiceCandidate",
    "p2p_juice_done": "reflectedWireJuiceDone",
}

GO_P2P_SERVER = {
    "server_p2p_connection_request": "crowserveWireConnectionRequest",
    "server_p2p_pong": "crowserveWirePong",
    "server_p2p_juice_local_description": "crowserveWireJuiceLocalDescription",
    "server_p2p_juice_candidate": "crowserveWireJuiceCandidate",
    "server_p2p_juice_done": "crowserveWireJuiceDone",
}


if __name__ == "__main__":
    sys.exit(main())