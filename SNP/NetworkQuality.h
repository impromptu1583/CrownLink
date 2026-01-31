#pragma once
#include "../Ema.h"

static constexpr auto PING_EVERY = 15;
static constexpr auto LATENCY_SAMPLES = 10;
static constexpr auto QUALITY_SAMPLES = 50;
static constexpr f64 DUPLICATE_SEND_THRESHOLD = 0.7;