#pragma once
#include <concurrentqueue.h>

#include <memory>

#include "CrowServeManager.h"
#include "JuiceManager.h"
#include "../types.h"
#include "../NetShared/StormTypes.h"

constexpr auto MAX_PACKET_SIZE = 500;
inline NetworkInfo g_network_info{
    (char*)"CrownLink",
    'CLNK',
    (char*)"",

    // CAPS: this is completely overridden by the appended .MPQ but storm tests to see if it's here anyway
    {sizeof(Caps), 0x20000003, MAX_PACKET_SIZE, 16, 256, 1000, 50, TurnsPerSecond::Standard, 2}
};

class Context {
public:
    void set_receive_event(handle event) { m_receive_event = event; };
    void set_client_info(ClientInfo* client_info) { m_client_info = *client_info; };

    auto& receive_event() { return m_receive_event; };
    auto& receive_queue() { return m_receive_queue; };
    auto& juice_manager() { return m_juice_manager; };
    auto& crowserve() { return m_crowserve_manager; };
    auto& client_info() { return m_client_info; };

private:
    handle m_receive_event;
    moodycamel::ConcurrentQueue<GamePacket> m_receive_queue;
    JuiceManager m_juice_manager;
    CrowServeManager m_crowserve_manager;
    ClientInfo m_client_info;
};

inline std::unique_ptr<Context> g_context;