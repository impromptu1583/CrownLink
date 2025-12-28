#pragma once
#include "JuiceAgent.h"
#include "../shared_common.h"
#include "../NetShared/StormTypes.h"
#include "Logger.h"

#include <unordered_map>
#include <memory>
#include <mutex>

class JuiceManager {
public:
    JuiceAgent* maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);
    JuiceAgent& ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);

    void clear_inactive_agents();
    void disconnect_if_inactive(const NetAddress& address);
    bool send_p2p(const NetAddress& address, const char* data, size_t size);
    void send_all(const char* data, size_t size);
    void send_connection_request(const NetAddress& address);
    void set_ice_credentials(const CrownLinkProtocol::IceCredentials& ice_credentials);

    template <typename T>
    void handle_crownlink_message(const T& message) {
        std::lock_guard lock{m_mutex};

        auto& peer_agent = ensure_agent(message.header.peer_id, lock);
        peer_agent.handle_crownlink_message(message);
    };

    juice_state lobby_agent_state(const AdFile& ad);
    JuiceConnectionType final_connection_type(const NetAddress& address);

    std::mutex& mutex() { return m_mutex; }

private:
    std::unordered_map<NetAddress, std::unique_ptr<JuiceAgent>> m_agents;
    std::mutex m_mutex;
    CrownLinkProtocol::IceCredentials m_ice_credentials{};
};

inline std::unique_ptr<JuiceManager> g_juice_manager;