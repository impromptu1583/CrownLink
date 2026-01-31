#pragma once
#include "Ema.h"

static constexpr auto PING_EVERY = 15;
static constexpr auto LATENCY_SAMPLES = 10;
static constexpr auto QUALITY_SAMPLES = 50;
static constexpr f32 DUPLICATE_SEND_THRESHOLD = 0.8;
static constexpr f32 LATENCY_THRESHOLD = 10.0f;
static constexpr f32 MAXIMUM_LATENCY_FOR_REDUNDANT = 1000/8*5; // turns/sec * 5