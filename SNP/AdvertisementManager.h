#pragma once
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <algorithm>

#include "../types.h"
#include "../NetShared/StormTypes.h"

enum class LogLevel;
struct SnpConfig;

class AdvertisementManager {
public:
    static AdvertisementManager& instance() {
        static AdvertisementManager manager;
        return manager;
    };

    void request_advertisements();
    void start_advertising(
        const char* game_name, const char* game_stat_string, u32 game_state, void* user_data, u32 user_data_size
    );
    void send_advertisement();
    void stop_advertising();
    bool in_games_list() const;
    void update_lobbies(std::vector<AdFile>& out_list);
    
    static void create_ad(
        AdFile& ad_file, const char* game_name, const char* game_stat_string, u32 game_state, void* user_data,
        u32 user_data_size
    );

    bool lock_game_list(u32 category_bits, u32 category_mask, AdFile** out_game_list);
    bool unlock_game_list() const;
    std::optional<std::reference_wrapper<const AdFile>> get_ad_by_index(u32 index) const;

    void iterate_lobby_list(void (*callback)(AdFile*, ConnectionState peer_state, void*), void* user_data);

    void set_status_ad(const std::string& status);
    void clear_status_ad();

    bool is_advertising() const;

    void use_status_add(bool ad_used) { m_status_ad_used = ad_used; };
    void edit_game_name(bool edit) { m_edit_game_name = edit; };

    void set_lobby_password(const char* new_password);
    void get_lobby_password(char* output, u32 output_size);

private:
    AdvertisementManager();
    void create_status_ad();
    void update_status_ad();
    ConnectionState connection_type(AdFile& lobby);
    void prune_lobbies_older_than(u32 seconds);

private:
    AdFile m_ad_data;

    bool m_is_advertising = false;

    std::chrono::steady_clock::time_point m_last_solicitation = std::chrono::steady_clock::now();
    mutable std::shared_mutex m_ad_mutex;

    u32 m_ellipsis_counter = 3;

    mutable std::recursive_mutex m_gamelist_mutex;

    std::vector<AdFile> m_lobbies;
    AdFile m_status_ad{};

    std::string m_lobby_password{};

    std::string m_status_string{};
    bool m_status_ad_used = true;
    bool m_edit_game_name = true;
};
