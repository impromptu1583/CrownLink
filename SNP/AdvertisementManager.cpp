#include "AdvertisementManager.h"
#include "CrowServeManager.h"

AdvertisementManager::AdvertisementManager() {
    create_status_ad();
}

bool AdvertisementManager::advertising() const {
    std::shared_lock lock{m_ad_mutex};
    return m_is_advertising;
}

void AdvertisementManager::request_advertisements() {
    spdlog::trace("Requesting lobbies");
    std::unique_lock lock{m_ad_mutex};
    auto request = CrownLinkProtocol::AdvertisementsRequest{{}, m_lobby_password};
    g_crowserve->socket().send_messages(CrowServe::ProtocolType::ProtocolCrownLink, request);

    switch (g_crowserve->socket().state()) {
        case CrowServe::SocketState::Ready: {
            clear_status_ad();
        } break;
        default: {
            auto status_string = std::string{"Connecting"};
            m_ellipsis_counter = (m_ellipsis_counter + 1) % 4;
            for (u32 i = 0; i < m_ellipsis_counter; i++) {
                status_string += ".";
            }
            set_status_ad(status_string);
        } break;
    }

    m_last_solicitation = std::chrono::steady_clock::now();
}

void AdvertisementManager::start_advertising(
    const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data, DWORD user_data_size
) {
    std::unique_lock lock{m_ad_mutex};

    create_ad(m_ad_data, game_name, game_stat_string, game_state, user_data, user_data_size);
    m_ad_data.turns_per_second = g_turns_per_second;
    m_is_advertising = true;
}

void AdvertisementManager::send_advertisement() {
    std::unique_lock lock{m_ad_mutex};

    if (m_ad_data.game_info.host != g_crowserve->socket().id()) {
        m_ad_data.game_info.host = g_crowserve->socket().id();
    }

    auto message = CrownLinkProtocol::StartAdvertising{{}, m_ad_data, m_lobby_password};
    g_crowserve->socket().send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);
}

void AdvertisementManager::stop_advertising() {
    auto message = CrownLinkProtocol::StopAdvertising{};
    g_crowserve->socket().send_messages(CrowServe::ProtocolType::ProtocolCrownLink, message);

    std::unique_lock lock{m_ad_mutex};
    m_is_advertising = false;
}

bool AdvertisementManager::in_games_list() const {
    std::shared_lock lock{m_ad_mutex};
    return (m_is_advertising && m_ad_data.game_info.game_state != 12) ||
           std::chrono::steady_clock::now() - m_last_solicitation < 2s;
}

void AdvertisementManager::update_lobbies(std::vector<AdFile>& updated_list) {
    std::lock_guard lock{m_gamelist_mutex};

    for (auto& known_lobby : m_lobbies) {
        auto it =
            std::ranges::find_if(updated_list.begin(), updated_list.end(), [&known_lobby](const AdFile& incoming_ad) {
                return known_lobby.is_same_owner(incoming_ad);
            });
        if (it != updated_list.end()) {
            known_lobby = *it;
            it->mark_for_removal = true;
        } else {
            known_lobby.mark_for_removal = true;
        }
    }

    m_lobbies.erase(
        std::remove_if(
            m_lobbies.begin(), m_lobbies.end(),
            [](const AdFile& ad) {
                return ad.mark_for_removal;
            }
        ),
        m_lobbies.end()
    );
    
    std::ranges::copy_if(updated_list, std::back_inserter(m_lobbies), [](const AdFile& incoming_ad) {
        return !incoming_ad.mark_for_removal;
    });
    
    const auto last_updated = get_tick_count();
    u32 index = 1;
    for (AdFile& known_lobby : m_lobbies) {
        known_lobby.game_info.host_last_time = last_updated;
        known_lobby.game_info.game_index = ++index;
        known_lobby.game_info.pExtra = known_lobby.extra_bytes;
    }
}

void AdvertisementManager::create_ad(
    AdFile& ad_file, const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data,
    DWORD user_data_size
) {
    ad_file = {};
    ad_file.turns_per_second = (TurnsPerSecond)g_turns_per_second;
    auto& game_info = ad_file.game_info;
    strcpy_s(game_info.game_name, sizeof(game_info.game_name), game_name);
    strcpy_s(game_info.game_description, sizeof(game_info.game_description), game_stat_string);
    game_info.game_state = game_state;
    game_info.program_id = g_client_info.program_id;
    game_info.version_id = g_client_info.version_id;
    game_info.host_latency = 0x0050;
    game_info.category_bits = 0x00a7;

    memcpy(ad_file.extra_bytes, user_data, user_data_size);
    game_info.extra_bytes = user_data_size;
    game_info.pExtra = ad_file.extra_bytes;
}

static std::string_view extract_map_name(const GameInfo& game_info) {
    std::string_view sv{game_info.game_description};
    sv.remove_prefix(std::min(sv.find('\r') + 1, sv.size()));
    auto back_trim_pos = sv.find_last_not_of("\t\n\v\f\r ");
    if (back_trim_pos != sv.npos) {
        sv.remove_suffix(sv.size() - back_trim_pos - 1);
    }
    return sv;
}

bool AdvertisementManager::lock_game_list(DWORD category_bits, DWORD category_mask, AdFile** out_game_list) {
    request_advertisements();

    // Storm will unlock this when it's done reading the games list by calling unlock_game_list
    m_gamelist_mutex.lock();

    if (!m_status_ad_used && m_lobbies.empty()) {
        // If we return false Storm won't call spi_unlock_game_list() to unlock the mutex
        m_gamelist_mutex.unlock();
        return false;
    }

    const auto& snp_config = SnpConfig::instance();

    update_status_ad();
    auto last_ad = &m_status_ad;

    std::erase_if(m_lobbies, [](const AdFile& lobby) {
        return get_tick_count() - lobby.game_info.host_last_time > 2;
    });

    for (auto& ad : m_lobbies) {
        bool joinable = true;
        bool has_text_prefix = false;
        std::stringstream ss;

        if (g_client_info.version_id != ad.game_info.version_id) {
            joinable = false;
            has_text_prefix = true;
            ss << ColorByte::Red << "[!Ver]";
        }
        if (ad.game_info.game_state == 12) {
            // Game in progress - these won't be shown anyway but we don't want to initiate p2p
            joinable = false;
            ss << ColorByte::Red;
        }

        // agent_state calls ensure_agent so one will be created if needed,
        // this kicks off the p2p connection process
        if (joinable && g_juice_manager) {
            switch (g_juice_manager->lobby_agent_state(ad)) {
                case JUICE_STATE_CONNECTING: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "...";
                } break;
                case JUICE_STATE_FAILED: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "!!";
                    g_juice_manager->send_connection_request(ad.game_info.host);
                } break;
                case JUICE_STATE_DISCONNECTED: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "-";
                    g_juice_manager->send_connection_request(ad.game_info.host);
                } break;
                case JUICE_STATE_CONNECTED:
                case JUICE_STATE_COMPLETED: {
                    // Use ColorByte to manipulate menu order but revert to standard menu behavior
                    ss << ColorByte::Green << ColorByte::Revert;
                    switch (g_juice_manager->final_connection_type(ad.game_info.host)) {
                        case JuiceConnectionType::Relay: {
                            has_text_prefix = true;
                            ss << "[R]";
                        } break;
                        case JuiceConnectionType::Radmin: {
                            has_text_prefix = true;
                            static constexpr char SadEmoji = 131;
                            ss << std::format("[Radmin {}]", SadEmoji);
                        } break;
                        default: {
                            has_text_prefix = true;
                            static constexpr char DoubleArrow = 187;
                            ss << DoubleArrow;
                        } break;
                    }
                }
            }
        }

        switch (ad.turns_per_second) {
            case TurnsPerSecond::UltraLow:
            case TurnsPerSecond::CLDB: {
                has_text_prefix = true;
                ss << "DB";
                break;
            }
            case TurnsPerSecond::Low: {
                has_text_prefix = true;
                ss << "T6";
                break;
            }
            case TurnsPerSecond::Medium: {
                has_text_prefix = true;
                ss << "T10";
                break;
            }
            case TurnsPerSecond::High: {
                has_text_prefix = true;
                ss << "T12";
                break;
            }
        }

        if (ad.original_name.empty()) {
            ad.original_name = ad.game_info.game_name;
        }
        ss << (has_text_prefix ? " " : "") << ad.original_name;

        if (snp_config.add_map_to_lobby_name) {
            ss << (joinable ? ColorByte::Blue : ColorByte::Revert) << " " << extract_map_name(ad.game_info);
        }

        const auto prefixes = ss.str();
        spdlog::debug("Lobby name updated to {}, length: {}", prefixes, prefixes.size());
        if (m_edit_game_name) {
            strncpy(ad.game_info.game_name, prefixes.c_str(), sizeof(ad.game_info.game_name));
        }

        if (last_ad) {
            last_ad->game_info.pNext = &ad.game_info;
        }
        last_ad = &ad;
    }

    if (last_ad) {
        last_ad->game_info.pNext = nullptr;
    }
    if (m_status_ad_used) {
        *out_game_list = &m_status_ad;
        return true;
    } else if (m_lobbies.size()) {
        *out_game_list = &m_lobbies[0];
        return true;
    }
    *out_game_list = nullptr;
    return false;
}

bool AdvertisementManager::unlock_game_list() {
    m_gamelist_mutex.unlock();
    return true;
}

bool AdvertisementManager::game_info_by_index(u32 index, GameInfo* output) {
    std::lock_guard lock{m_gamelist_mutex};
    for (auto& ad : m_lobbies) {
        if (ad.game_info.game_index == index) {
            *output = ad.game_info;
            if (ad.turns_per_second == TurnsPerSecond::CNLK) {
                g_current_provider->caps.turns_per_second = 8;
            } else if (ad.turns_per_second == TurnsPerSecond::CLDB) {
                g_current_provider->caps.turns_per_second = 4;
            } else {
                g_current_provider->caps.turns_per_second = ad.turns_per_second;
            }
            return true;
        }
    }
    return false;
}

void AdvertisementManager::set_status_ad(const std::string& status) {
    m_status_string = status;
}

void AdvertisementManager::clear_status_ad() {
    m_status_string.clear();
}

void AdvertisementManager::set_lobby_password(const char* new_password) {
    std::unique_lock lock{m_ad_mutex};
    m_lobby_password = new_password;
}

void AdvertisementManager::get_lobby_password(char* output, u32 output_size) {
    std::shared_lock lock{m_ad_mutex};
    strcpy_s(output, output_size, m_lobby_password.c_str());
}

void AdvertisementManager::create_status_ad() {
    std::lock_guard lock{m_gamelist_mutex};
    char user_data[32] = {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    create_ad(m_status_ad, "", ",33,,3,,1e,,1,cb2edaab,5,,Server\rStatus\r", 12, &user_data, sizeof(user_data));
    m_status_ad.game_info.game_index = 1;
    m_status_ad.game_info.pNext = nullptr;
}

void AdvertisementManager::update_status_ad() {
    const auto& snp_config = SnpConfig::instance();

    std::string output{};
    // "start of text" character changes to a heading font and makes sure the status is at the top of the list
    if (m_status_string.empty()) {
        if (!snp_config.lobby_password.empty()) {
            output += std::format("{}Private ", char(ColorByte::Blue));
        }
        if (g_turns_per_second == TurnsPerSecond::CLDB || g_turns_per_second == TurnsPerSecond::UltraLow) {
            if (output.empty()) {
                output += std::format("{}", char(ColorByte::Blue));
            }
            output += "DBC ";
        }
        if (!output.empty()) {
            output += "Mode";
        }
    } else {
        output = std::format("{}{}", char(ColorByte::LightGreen), m_status_string);
        // intentionally not using a "start of text" character here so the ad is green
    }
    if (output.empty()) {
        m_status_ad.game_info.game_state = 12;  // hide the ad, games list doesn't show games "in progress"
        return;
    }
    m_status_ad.game_info.game_state = 0;  // show the ad
    strcpy_s(m_status_ad.game_info.game_name, sizeof(m_status_ad.game_info.game_name), output.c_str());
    strcpy_s(m_status_ad.game_info.game_name, sizeof(m_status_ad.game_info.game_name), output.c_str());
}