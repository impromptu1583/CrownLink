#pragma once
#include "../Ema.h"

static constexpr auto PING_EVERY = 15;
static constexpr auto LATENCY_SAMPLES = 10;
static constexpr auto QUALITY_SAMPLES = 50;
static constexpr f64 DUPLICATE_SEND_THRESHOLD = 0.7;

class NetworkQualityTracker {
public:
    void record_packet_sent() { ++m_send_count; }
    void record_successful_packet() { m_average_quality.update(1.0); }
    void record_resend_request() { m_average_quality.update(0.0); }
    void record_ping_response(s64 rtt_ms) { m_average_latency.update(rtt_ms); }

    bool should_send_ping() const { return m_send_count % PING_EVERY == 0; }
    bool should_send_duplicate() const {
        if (m_average_quality > DUPLICATE_SEND_THRESHOLD) return false;

        // TODO: Add dll export to allow client to pass updated values
        // For now use defaults
        auto turns_per_second = 8;
        auto turns_in_transit = 2;
        auto ms_per_turn = 1000 / turns_per_second;
        auto max_latency = ms_per_turn * turns_in_transit;
        auto max_round_trip = max_latency * 2 + 20;

        return m_average_latency <= max_round_trip;
    }

    f32 get_quality() const { return m_average_quality; }
    f32 get_latency() const { return m_average_latency; }
      
private:
    EMA m_average_latency{LATENCY_SAMPLES};
    EMA m_average_quality{QUALITY_SAMPLES, 1.0};
    u32 m_send_count = 0;

};