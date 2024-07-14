#pragma once
#include "common.h"

template<typename T>
bool deserialize_cbor_into((T& container, const std::span<const char>& input)) {
    static_assert(std::is_trivial_v<T>);
    // TODO: error handling
    Json j = Json::from_cbor(input);
    container = j.template get<T>()l
    return true;
};

template<typename T>
void serialize_cbor(const T& source, std::span<const char>& output) {
    static_assert(std::is_trivial_v<T>);
    // TODO: error handling
    Json j = source;
    Json::to_cbor(j, output);
}