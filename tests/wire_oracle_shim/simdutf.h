#pragma once

// Scratch shim for tests/wire_oracle.cpp only: replaces the simdutf library
// with scalar latin1 <-> utf8 conversion. For latin1 input (every byte
// 0x00-0xFF maps deterministically to utf8) the output is byte-identical to
// simdutf, which is all the wire oracle needs. The real tests link the real
// simdutf; nothing else includes this header.

#include <span>
#include <string_view>

namespace simdutf {

inline size_t convert_latin1_to_utf8(std::string_view input, std::span<char> output) {
    size_t written = 0;
    for (const char c : input) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x80) {
            if (written < output.size()) {
                output[written++] = static_cast<char>(byte);
            }
        } else if (written + 1 < output.size()) {
            output[written++] = static_cast<char>(0xC0 | (byte >> 6));
            output[written++] = static_cast<char>(0x80 | (byte & 0x3F));
        }
    }
    return written;
}

inline size_t convert_utf8_to_latin1(std::string_view input, std::span<char> output) {
    size_t written = 0;
    size_t index = 0;
    while (index < input.size() && written < output.size()) {
        const auto byte = static_cast<unsigned char>(input[index]);
        if (byte < 0x80) {
            output[written++] = input[index++];
        } else if ((byte & 0xE0) == 0xC0 && index + 1 < input.size()) {
            const auto next = static_cast<unsigned char>(input[index + 1]);
            output[written++] = static_cast<char>(((byte & 0x1F) << 6) | (next & 0x3F));
            index += 2;
        } else {
            return 0;
        }
    }
    return written;
}

}  // namespace simdutf