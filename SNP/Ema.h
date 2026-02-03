#pragma once
#include "../types.h"
#include <optional>
#include <atomic>

// Exponential Moving Average
class EMA {
public:
    EMA(s32 n, std::optional<f32> initial_value = {}) : alpha{(f32)2.0 / (n + 1)} { 
        if (initial_value) {
            average = *initial_value;
            initialized = true;
        }
    }

    f32 update(f32 newValue) {
        if (!initialized) {
            average = newValue;
            initialized = true;
        } else {
            average += alpha * (newValue - average);
        }
        return average;
    }

    operator f32() const { return average; }

private:
    std::atomic<f32> average = 0.0;
    std::atomic<bool> initialized = false;
    const f32 alpha;
};