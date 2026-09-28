#pragma once
#include "common.h"

#include <rfl/cbor.hpp>

#include <iostream>
#include <span>
#include <vector>

template <typename T>
bool deserialize_cbor_into(T& container, std::span<u8> input) {
    const auto bytes = std::span<const char>{reinterpret_cast<const char*>(input.data()), input.size()};
    const auto result = rfl::cbor::read<T>(bytes);
    if (!result) {
        std::cout << "failed to deserialize message: " << result.error().what() << std::endl;
        return false;
    }
    container = std::move(result.value());
    return true;
};

template <typename T>
void serialize_cbor(const T& source, std::vector<u8>& output) {
    try {
        const auto bytes = rfl::cbor::write(source);
        output.assign(reinterpret_cast<const u8*>(bytes.data()), reinterpret_cast<const u8*>(bytes.data()) + bytes.size());
    } catch (const std::exception& e) {
        std::cout << "failed to serialize message: " << e.what() << std::endl;
        output.clear();
    }
}