#include "NetworkQuality.h"
#include "../Globals.h"

bool NetworkQualityTracker::should_send_duplicate() const {
    if (m_average_quality > DUPLICATE_SEND_THRESHOLD) return false;

    auto turns_per_second = g_network_info.caps.turns_per_second ? g_network_info.caps.turns_per_second : 8;
    auto turns_in_transit = g_network_info.caps.turns_in_transit ? g_network_info.caps.turns_in_transit : 2;
    auto ms_per_turn = 1000 / turns_per_second;
    auto max_latency = ms_per_turn * turns_in_transit;
    auto max_round_trip = max_latency * 2 + 20;

    return m_average_latency <= max_round_trip;
}
