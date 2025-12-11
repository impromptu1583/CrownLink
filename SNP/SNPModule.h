#pragma once
#include "Common.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace snp {

constexpr auto MAX_PACKET_SIZE = 500;

struct NetworkInfo {
    char* name;
    DWORD id;
    char* description;
    Caps caps;
};

void update_lobbies(std::vector<AdFile> &updated_list);
void create_status_ad();
void update_status_ad();
void set_status_ad(const std::string &status);
void clear_status_ad();
void packet_parser(const GamePacket* game_packet);

struct NetFunctions {
    DWORD size; // 112 bytes
    BOOL(__stdcall* compare_net_addresses)(NetAddress* left, NetAddress* right, DWORD* result);
    BOOL(__stdcall* destroy)();
    BOOL(__stdcall* free_message)(NetAddress* address, char* data, DWORD size);
    BOOL(__stdcall* free_external_message)(NetAddress* address, char* data, DWORD size);
    BOOL(__stdcall* get_game_info)(DWORD index, const char* game_name_unused, const char* password_unused, GameInfo* output);
    BOOL(__stdcall* unused_get_performance_data)(DWORD type, DWORD* output, DWORD, DWORD);
    BOOL(__stdcall* initialize)(ClientInfo* gameClientInfo, UserInfo* userData, BattleInfo* bnCallbacks, ModuleInfo* moduleData, HANDLE hEvent);
    BOOL(__stdcall* unused_initialize_device)(int, void*, void*, DWORD*, void*);
    BOOL(__stdcall* unused_lock_device_list)(DWORD* out_device_list);
    BOOL(__stdcall* lock_game_list)(int, int, AdFile** out_game_list);
    BOOL(__stdcall* receive_message)(NetAddress** sender, char** message, DWORD* message_size);
    BOOL(__stdcall* receive_external_message)(NetAddress** sender, char** message, DWORD* message_size);
    BOOL(__stdcall* bnet_select_game)(
        DWORD flags, ClientInfo* program_info, UserInfo* player_info, BattleInfo* bnet_callbacks,
        ModuleInfo* version_info, DWORD* player_id);
    BOOL(__stdcall* send_message)(
        DWORD destination_count, NetAddress** destination_list, char* message, DWORD message_size);
    BOOL(__stdcall* send_external_message)(
        char* destination, DWORD message_size, char* blank1, char* blank2, char* message);
    BOOL(__stdcall* start_advertising)(
        const char* game_name, const char* game_password, const char* game_stat_string, DWORD game_state,
        DWORD elapsed_time, DWORD game_type, DWORD, DWORD, void* user_data, DWORD user_data_size);
    BOOL(__stdcall* stop_advertising)();
    BOOL(__stdcall* unused_unlock_device_list)(void*);
    BOOL(__stdcall* unlock_game_list)(AdFile* game_list, DWORD* list_count);
    BOOL(__stdcall* bnet_get_local_player_name)(char* name, DWORD name_size, char* description, DWORD description_size);
    BOOL(__stdcall* bnet_report_game_result)(
        DWORD unknown1, DWORD unknown2, void* unknown3, void* unknown4, void* unknown5);
    BOOL(__stdcall* unused_spi_check_data_file)();
    BOOL(__stdcall* bnet_send_command)(char* stats, void(__fastcall* callback)(char*, DWORD));
    BOOL(__stdcall* bnet_send_replay_path)(DWORD unknown1, DWORD unknown2, char* path);
    BOOL(__stdcall* bnet_unknown_replay)(DWORD unknown1);
    BOOL(__stdcall* bnet_logout)(const char* player_name);
    BOOL(__stdcall* bnet_get_player_name)(char* name_output, DWORD size);
};

extern NetFunctions g_spi_functions;

};  // namespace snp
