#pragma once

#include <string>
#include <utility>

#include <storm/types.hpp>

enum class TurnsPerSecond : u32 {
    UltraLow = 4,
    Low = 6,
    Standard = 8,
    Medium = 10,
    High = 12,
};

inline bool is_valid(TurnsPerSecond mode) {
    switch (mode) {
        case TurnsPerSecond::UltraLow:
        case TurnsPerSecond::Low:
        case TurnsPerSecond::Standard:
        case TurnsPerSecond::Medium:
        case TurnsPerSecond::High:
            return true;
    }
    return false;
}

inline std::string to_string(TurnsPerSecond value) {
    switch (value) {
        case TurnsPerSecond::UltraLow: return "UltraLow";
        case TurnsPerSecond::Low: return "Low";
        case TurnsPerSecond::Standard: return "Standard";
        case TurnsPerSecond::Medium: return "Medium";
        case TurnsPerSecond::High: return "High";
    }
    return std::to_string(std::to_underlying(value));
}