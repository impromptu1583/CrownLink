#pragma once
#include "common.h"
#include <iostream>

template <typename T>
bool deserialize_cbor_into(T& container, std::span<u8> input) {
    try {
        Json json = Json::from_cbor(input);
        container = json.template get<T>();
        return true;
    } catch (const Json::parse_error& e) {
        // TODO: improve logging
        std::cout << "message: " << e.what() << '\n'
                  << "exception id: " << e.id << '\n'
                  << "byte position of error: " << e.byte << std::endl;
        return false;
    }
};

template <typename T>
void serialize_cbor(const T& source, std::vector<u8>& output) {
    // TODO: error handling
    Json json = source;
    Json::to_cbor(json, output);
}