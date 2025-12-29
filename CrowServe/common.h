#pragma once

#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include "../types.h"
#include "Cbor.h"

using namespace std::literals;