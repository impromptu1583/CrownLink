#pragma once
#include "../types.h"
#include "../NetShared/StormTypes.h"

namespace snp {

constexpr auto MAX_PACKET_SIZE = 500;

struct NetworkInfo {
    char* name;
    u32 id;
    char* description;
    Caps caps;
};

inline NetworkInfo g_network_info{
    (char*)"CrownLink",
    'CLNK',
    (char*)"",

    // CAPS: this is completely overridden by the appended .MPQ but storm tests to see if it's here anyway
    {sizeof(Caps), 0x20000003, snp::MAX_PACKET_SIZE, 16, 256, 1000, 50, 8, 2}
};

void packet_parser(const GamePacket* game_packet);
bool set_turns_per_second(TurnsPerSecond turns_per_second);

TurnsPerSecond get_turns_per_second();

struct NetFunctions {
    u32 size; // 112 bytes
    b32(__stdcall* compare_net_addresses)(const NetAddress* left, const NetAddress* right, u32* result);
    b32(__stdcall* destroy)();
    b32(__stdcall* free_message)(GamePacket* address, char* data, u32 size);
    b32(__stdcall* free_external_message)(NetAddress* address, char* data, u32 size);
    b32(__stdcall* get_game_info)(u32 index, const char* game_name_unused, const char* password_unused, GameInfo* output);
    b32(__stdcall* unused_get_performance_data)(u32 type, u32* output, u32, u32);
    b32(__stdcall* initialize)(ClientInfo* gameClientInfo, UserInfo* userData, BattleInfo* bnCallbacks, ModuleInfo* moduleData, handle hEvent);
    b32(__stdcall* unused_initialize_device)(int, void*, void*, u32*, void*);
    b32(__stdcall* unused_lock_device_list)(u32* out_device_list);
    b32(__stdcall* lock_game_list)(u32 category_bits, u32 category_mask, AdFile** out_game_list);
    b32(__stdcall* receive_message)(NetAddress** sender, GamePacketData** message, u32* message_size);
    b32(__stdcall* receive_external_message)(NetAddress** sender, char** message, u32* message_size);
    b32(__stdcall* bnet_select_game)(
        u32 flags, ClientInfo* program_info, UserInfo* player_info, BattleInfo* bnet_callbacks,
        ModuleInfo* version_info, u32* player_id);
    b32(__stdcall* send_message)(
        u32 destination_count, NetAddress** destination_list, char* message, u32 message_size);
    b32(__stdcall* send_external_message)(
        char* destination, u32 message_size, char* blank1, char* blank2, char* message);
    b32(__stdcall* start_advertising)(
        const char* game_name, const char* game_password, const char* game_stat_string, u32 game_state,
        u32 elapsed_time, u32 game_type, u32, u32, void* user_data, u32 user_data_size);
    b32(__stdcall* stop_advertising)();
    b32(__stdcall* unused_unlock_device_list)(void*);
    b32(__stdcall* unlock_game_list)(AdFile* game_list, u32* list_count);
    b32(__stdcall* bnet_get_local_player_name)(char* name, u32 name_size, char* description, u32 description_size);
    b32(__stdcall* bnet_report_game_result)(
        u32 unknown1, u32 unknown2, void* unknown3, void* unknown4, void* unknown5);
    b32(__stdcall* unused_spi_check_data_file)();
    b32(__stdcall* bnet_send_command)(char* stats, void(__fastcall* callback)(char*, u32));
    b32(__stdcall* bnet_send_replay_path)(u32 unknown1, u32 unknown2, char* path);
    b32(__stdcall* bnet_unknown_replay)(u32 unknown1);
    b32(__stdcall* bnet_logout)(const char* player_name);
    b32(__stdcall* bnet_get_player_name)(char* name_output, u32 size);
};

extern NetFunctions g_spi_functions;

};  // namespace snp
