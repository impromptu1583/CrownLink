#include "CrowServeManager.h"
#include "AdvertisementManager.h"
#include "JuiceManager.h"
#include "Config.h"
#include "Globals.h"

CrowServeManager::CrowServeManager() {
    init_listener();
}

CrowServeManager::~CrowServeManager() {
    if (m_listener_thread.joinable()) {
        m_listener_thread.request_stop();
        m_listener_thread.join();
        // Brief delay to ensure socket polling has ended
        std::this_thread::sleep_for(100ms);
    }
}

void CrowServeManager::init_listener() {
    const auto& snp_config = SnpConfig::instance();
    m_socket.set_external_logger([](const std::string& message) {
        spdlog::info("Crowserve: {}", message);
    });
    m_listener_thread = m_socket.listen(
        snp_config.server, snp_config.port, snp_config.lobby_password,
        [&](const CrownLinkProtocol::ClientProfile& message) {
            spdlog::info("received client ID from server: {}", message.peer_id);
            g_context->juice_manager().set_ice_credentials(message.ice_credentials);
            m_socket.set_profile(message.peer_id, message.token);
            for (u32 i = 0; i < message.ice_credentials.turn_servers_count; i++) {
                auto& ts = message.ice_credentials.turn_servers[i];
                spdlog::info(
                    "Turn server received, host: {}, port: {}, user: {}, pass: {}", ts.host, ts.port, ts.username,
                    ts.password
                );
            }
        },
        [&](const CrownLinkProtocol::UpdateAvailable& message) {
            // TODO: Display status string
        },
        [&](const CrownLinkProtocol::AdvertisementsRequest& message) {
            spdlog::trace("Received server heartbeat");
            if (!AdvertisementManager::instance().in_games_list()) {
                g_context->juice_manager().clear_inactive_agents();
            }
            if (AdvertisementManager::instance().is_advertising()) {
                AdvertisementManager::instance().send_advertisement();
            } else {
                auto reply = CrownLinkProtocol::EchoResponse{{}, ""};
                m_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, reply);
            }
        },
        [&](CrownLinkProtocol::AdvertisementsResponse& message) {
            AdvertisementManager::instance().update_lobbies(message.ad_files);
        },
        [&](const CrownLinkProtocol::EchoRequest& message) {
            auto reply = CrownLinkProtocol::EchoResponse{{}, message.message};
            m_socket.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, reply);
        },
        [&](const P2P::ConnectionRequest& message) {
            g_context->juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::Pong& message) {
            g_context->juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceLocalDescription& message) {
            g_context->juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceCandidate& message) {
            g_context->juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceDone& message) {
            g_context->juice_manager().handle_crownlink_message(message);
        },
        [](const auto& message) {}
    );
}