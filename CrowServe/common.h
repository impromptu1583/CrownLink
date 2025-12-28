#pragma once

#define JSON_DIAGNOSTICS 1
#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include "../shared_common.h"
#include "Cbor.h"

using namespace std::literals;