#pragma once

#include "Common.h"
#include "Config.h"
#include "JuiceManager.h"

inline snp::NetworkInfo g_network_info{
    (char*)"CrownLink",
    'BNET',
    (char*)"",

    // CAPS: this is completely overridden by the appended .MPQ but storm tests to see if its here anyway
    {sizeof(Caps), 0x00000001, snp::MAX_PACKET_SIZE, 16, 256, 1000, 50, 8, 2}
};

class CrownLink {
public:
    CrownLink();
    ~CrownLink();

    CrownLink(const CrownLink&) = delete;
    CrownLink& operator=(const CrownLink&) = delete;

    void request_advertisements();
    bool send(const NetAddress& to, void* data, size_t size);
    void start_advertising(AdFile& ad_data);
    void send_advertisement();
    void stop_advertising();
    bool in_games_list() const;

    auto& advertising();
    auto& receive_queue() { return m_receive_queue; }
    auto& juice_manager() { return m_juice_manager; }
    auto& crowserve() { return m_crowserve; }

private:
    void init_listener();

private:
    moodycamel::ConcurrentQueue<GamePacket> m_receive_queue;

    CrowServe::Socket m_crowserve;
    std::jthread m_listener_thread;
    JuiceManager m_juice_manager;
    AdFile m_ad_data;

    bool m_is_advertising = false;
    bool m_is_running = true;

    std::chrono::steady_clock::time_point m_last_solicitation = std::chrono::steady_clock::now();
    mutable std::shared_mutex m_ad_mutex;

    u32 m_ellipsis_counter = 3;
};

inline HANDLE g_receive_event;
inline std::unique_ptr<CrownLink> g_crown_link;
inline std::mutex g_advertisement_mutex;
