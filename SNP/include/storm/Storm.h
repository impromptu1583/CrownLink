#pragma once
#include "../../shared_common.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

inline auto g_storm_dll = GetModuleHandleA("storm.dll");

template <typename>
class StormStdCall;

template <typename Result, typename... Args>
class StormStdCall<Result(Args...)> {
public:
    using FuncPtr = Result (__stdcall *)(Args...);

    StormStdCall(const char* name) : m_func{reinterpret_cast<FuncPtr>(GetProcAddress(g_storm_dll, name))} {}

    StormStdCall(u32 ordinal)
        : m_func{reinterpret_cast<FuncPtr>(GetProcAddress(g_storm_dll, MAKEINTRESOURCE(ordinal)))} {}

    Result operator()(Args... args) const { return m_func(args...); }

private:
    FuncPtr m_func;
};

inline const StormStdCall<s32()> SErrGetLastError{463};
inline const StormStdCall<b32(u32 error_code, char* buffer, size_t buffer_size)> SErrGetErrorStr{462};
inline const StormStdCall<b32(u32 game_id, char* game_name, char* game_pw,
    char* player_name, char* player_description, int* player_id)> SNetJoinGame{118};
//inline const StormStdCall<b32(char* game_name, char* game_password, char* game_description,
//    u32 game_category_bits, char* init_data, u32 init_data_size, u32 max_players, char* player_name, char* player_description, DWORD* player_id)> SNetCreateGame{101};

inline const StormStdCall<b32(char* gamename, char* password, char* description, u32 category_bits,
    void* initdata, u32 initdata_size, u32 max_players, char* player_name, char* player_description, DWORD* playerid)> SNetCreateGame{101};

//inline char* bw_player_name = reinterpret_cast<char*>(0x0057ee9c);
//inline u32* G_playerid = reinterpret_cast<u32*>(0x0051268c);

#define BW_DATA(type, name, offset) \
    type const name =               \
        (type)offset;  // Designed as const to make sure programmers don't read from or write to the address itself.
#define BW_DATA_REF(type, name, offset) \
    extern type& name;  // Contains the definitions of various data structures, arrays, pointers and internal constants
                        // in StarCraft.exe.

struct
    GameData  // taken from IDA and from
                 // https://gist.githubusercontent.com/neivv/d56baedfdfa0145575825e160b64adb7/raw/1bfc4e2113966eac0749f7f02de3fa9a091fb84b/nuottei.txt
{
    u32 save_time;                 // 0x0
    char game_name[0x18];          // 0x4
    u32 save_timestamp;            // 0x1C
    u16 map_width_tiles;           // 0x20
    u16 map_height_tiles;          // 0x22
    u8 active_player_count;        // 0x24
    u8 max_player_count;           // 0x25
    u8 game_speed;                 // 0x26
    u8 game_state;                 // 0x27
    u16 g_game_type;               // 0x28
    u16 game_subtype;              // 0x2A
    u32 cdkey_hash;                // 0x2C
    u16 tileset;                   // 0x30
    u8 is_replay;                  // 0x32
    u8 computer_player_count;      // 0x33
    u8 host_name[0x19];            // 0x34
    char map_title[0x20];          // 0x4D
    u16 got_game_type;             // 0x6D
    u16 got_game_subtype;          // 0x6F
    u16 got_game_subtype_display;  // 0x71
    u16 got_game_subtype_label;    // 0x73
    u8 victory_condition;          // 0x75
    u8 resource_type;              // 0x76
    u8 use_standard_unit_stats;    // 0x77
    u8 fog_of_war;                 // 0x78
    u8 starting_units;             // 0x79
    u8 use_fixed_positions;        // 0x7A
    u8 restriction_flags;          // 0x7B
    u8 allies_enabled;             // 0x7C
    u8 teams_enabled;              // 0x7D
    u8 cheats_enabled;             // 0x7E
    u8 tournament_mode_enabled;    // 0x7F
    u32 victory_condition_value;   // 0x80
    u32 resource_mineral_value;    // 0x84
    u32 resource_gas_value;        // 0x88
};

BW_DATA(char*, bw_player_name, 0x0057ee9c);
BW_DATA(u32*, bw_player_id, 0x0051268c);

BW_DATA(char*, bw_starcraft_exe_path, 0x0068f718);
BW_DATA(char*, bw_maps_directory, 0x0059bf78);

BW_DATA(char*, bw_current_map_filename, 0x0057FD3C);
BW_DATA(char*, bw_current_map_name, 0x0057FE40);

BW_DATA(GameData*, bw_game_data, 0x005967F8);
BW_DATA(BOOL*, bw_is_host, 0x00596888);

