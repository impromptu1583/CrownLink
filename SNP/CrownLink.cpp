#include "CrownLink.h"

CrownLink::CrownLink() {
    spdlog::info("Crownlink loaded, version {}", CL_VERSION);
    m_is_running = true;
    init_listener();
}

CrownLink::~CrownLink() {
    spdlog::info("Shutting down");
    m_is_running = false;
    m_listener_thread.request_stop();
    auto echo = CrownLinkProtocol::EchoRequest{};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, echo);
}

bool CrownLink::in_games_list() const {
    std::shared_lock lock{m_ad_mutex};

    return (m_is_advertising && m_ad_data.game_info.game_state == 0) || std::chrono::steady_clock::now() - m_last_solicitation < 2s;
}

auto& CrownLink::advertising() {
    std::shared_lock lock{m_ad_mutex};
    return m_is_advertising;
}

void CrownLink::request_advertisements() {
    spdlog::debug("Requesting lobbies");
    auto request = CrownLinkProtocol::AdvertisementsRequest{};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, request);

    switch (m_crowserve.state()) {
        case CrowServe::SocketState::Ready: {
            snp::clear_status_ad();
        } break;
        default: {
            auto status_string = std::string{"CrownLink Connecting"};
            m_ellipsis_counter = (m_ellipsis_counter + 1) % 4;
            for (u32 i = 0; i < m_ellipsis_counter; i++) {
                status_string += ".";
            }
            snp::set_status_ad(status_string);
        } break;
    }

    std::unique_lock lock{m_ad_mutex};
    m_last_solicitation = std::chrono::steady_clock::now();
}

bool CrownLink::send(const NetAddress& peer, void* data, size_t size) {
    return m_juice_manager.send_p2p(peer, data, size);
}

void CrownLink::init_listener() {
    const auto& snp_config = SnpConfig::instance();
    m_crowserve.external_logger = [](const std::string message) {
        spdlog::info("Crowserve: {}", message);
    };
    m_listener_thread = m_crowserve.listen(
        snp_config.server,
        snp_config.port,
        [&](const CrownLinkProtocol::ClientProfile& message) {
            spdlog::info("received client ID from server: {}", message.peer_id);
            set_id(message.peer_id);
            set_token(message.token);
            juice_manager().set_ice_credentials(message.ice_credentials);
            // TODO clean this up
            crowserve().set_profile(message.peer_id, message.token);
        },
        [&](const CrownLinkProtocol::UpdateAvailable& message) {
            // TODO display status string
        },
        [&](const CrownLinkProtocol::AdvertisementsRequest& message) {
            spdlog::trace("Received server heartbeat");
            if (!in_games_list()) {
                juice_manager().clear_inactive_agents();
            }
            if (advertising()) {
                send_advertisement();
            }
        },
        [&](CrownLinkProtocol::AdvertisementsResponse& message) {
            snp::update_lobbies(message.ad_files);
        },
        [&](const CrownLinkProtocol::EchoRequest& message) {
            auto reply = CrownLinkProtocol::EchoResponse{{}, message.message};
            crowserve().send_messages(CrowServe::ProtocolType::ProtocolCrownLink, reply);
        },
        [&](const P2P::Ping& message) {
            juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::Pong& message) {
            juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceLocalDescription& message) {
            juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceCandidate& message) {
            juice_manager().handle_crownlink_message(message);
        },
        [&](const P2P::JuiceDone& message) {
            juice_manager().handle_crownlink_message(message);
        },
        [](const auto& message) {}
    );
}

void CrownLink::start_advertising(AdFile ad_data) {
    std::unique_lock lock{m_ad_mutex};
    m_ad_data = ad_data;

    m_is_advertising = true;
}

void CrownLink::send_advertisement() {
    std::unique_lock lock{m_ad_mutex};

    if (m_ad_data.game_info.host != m_client_id) {
        m_ad_data.game_info.host = m_client_id;
    }

    auto message = CrownLinkProtocol::StartAdvertising{{}, m_ad_data};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);
}

void CrownLink::stop_advertising() {
    auto message = CrownLinkProtocol::StopAdvertising{};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);
    spdlog::info("Stopped advertising lobby");

    std::unique_lock lock{m_ad_mutex};
    m_is_advertising = false;
}
