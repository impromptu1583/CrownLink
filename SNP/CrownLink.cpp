#include "CrownLink.h"

CrownLink::CrownLink() {
    spdlog::info("Initializing, version {}", CL_VERSION);
    m_is_running = true;
}

CrownLink::~CrownLink() {
    spdlog::info("Shutting down");
    m_is_running = false;
    // TODO: how to clean up crowserve properly?

    spdlog::debug("Receive thread closed");
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
}

bool CrownLink::send(const NetAddress& peer, void* data, size_t size) {
    return m_juice_manager.send_p2p(peer, data, size);
}

void CrownLink::init_listener() {
    const auto& snp_config = SnpConfig::instance();
    m_crowserve.listen(
        snp_config.server,
        snp_config.port,
        [&](const CrownLinkProtocol::ClientProfile& message) {
            spdlog::info("received client ID from server: {}", message.peer_id);
            set_id(message.peer_id);
            set_token(message.token);
            juice_manager().set_ice_credentials(message.ice_credentials);
        },
        [&](const CrownLinkProtocol::UpdateAvailable& message) {
            // TODO display status string
        },
        [&](const CrownLinkProtocol::AdvertisementsRequest& message) {
            if (advertising()) {
                send_advertisement();
            }
        },
        [&](const CrownLinkProtocol::AdvertisementsResponse& message) {
            pass_advertisements(message.ad_files);
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
    m_ad_data = ad_data;
    m_is_advertising = true;
    send_advertisement();
}

void CrownLink::send_advertisement() {
    if (!m_client_id_set) {
        return;
    }
    if (m_ad_data.game_info.host != m_client_id) {
        m_ad_data.game_info.host = m_client_id;
    }
    auto message = CrownLinkProtocol::StartAdvertising{{}, m_ad_data};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);
    spdlog::debug("sending advertisement");
}

void CrownLink::stop_advertising() {
    m_is_advertising = false;
    auto message = CrownLinkProtocol::StopAdvertising{};
    m_crowserve.send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);
    spdlog::info("Stopped advertising lobby");
}

void CrownLink::pass_advertisements(const std::vector<AdFile>& advertisements) {
    for (auto ad : advertisements) {
        snp::pass_advertisement(ad);
    }
}
