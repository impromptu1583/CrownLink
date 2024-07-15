#pragma once
#include "common.h"

template<typename T>
bool deserialize_cbor_into(T& container, std::span<const char> input) {
    // TODO: error handling
    Json json = Json::from_cbor(input);
    container = json.template get<T>();
    return true;
};

template<typename T>
bool deserialize_cbor_into(T& container, std::vector<u8>& input) {
    // TODO: error handling
    Json json = Json::from_cbor(input);
    container = json.template get<T>();
    return true;
};

template<typename T>
void serialize_cbor(const T& source, std::vector<u8>& output) {
    // TODO: error handling
    Json json = source;
    Json::to_cbor(json, output);
}