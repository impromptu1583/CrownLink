#pragma once
#include <concurrentqueue.h>

#include <memory>

#include "CrowServeManager.h"
#include "JuiceManager.h"
#include "shared/types.h"
#include "shared/StormTypes.h"

constexpr auto caps_flags =
    std::to_underlying(CapsFlags::PageLockedBuffers) | std::to_underlying(CapsFlags::BasicInterface) |
    std::to_underlying(CapsFlags::ReleaseMode);

inline NetworkInfo g_network_info{
    (char*)"CrownLink",
    'CNLK',
    (char*)"",

    // CAPS: this is completely overridden by the appended .MPQ but storm tests to see if it's here anyway
    {sizeof(Caps), caps_flags, MaxPacketSize, 16, 256, 1000, 50, std::to_underlying(TurnsPerSecond::Standard), 2}
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