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
inline const StormStdCall<b32(u32 game_id, const char* game_name, const char* game_pw,
    const char* player_name, const char* player_description, u32* player_id)> SNetJoinGame{118};
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
    u32 g_game_type;               // 0x28
    //u16 game_subtype;              // 0x2A
    u32 cdkey_hash;                // 0x2C
    u16 tileset;                   // 0x30
    u8 is_replay;                  // 0x32
    u8 computer_player_count;      // 0x33
    u8 host_name[0x19];            // 0x34
    char map_title[0x19];          // 0x4D
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

BW_DATA(u64*, bw_force_listing, 0x0058d5b0);
BW_DATA(u32*, bw_force_flags, 0x0058d5b8);

BW_DATA(u16*, bw_game_type, 0x00596820);
BW_DATA(u16*, bw_game_subtype, 0x00596822);

//BW_DATA(u32* bw_game_speed, 0x006cdfd4);

struct Options  // taken from IDA
{
    u32 speed;              // 0x0
    u32 mouse_scroll;       // 0x4
    u32 keyboard_scroll;    // 0x8
    u32 music;              // 0xC
    u32 sfx;                // 0x10
    u32 tip_number;         // 0x14
    u32 flags;              // 0x18
    u32 m_mouse_scroll;     // 0x1C
    u32 m_keyboard_scroll;  // 0x20
    u32 use_chat_colors;    // 0x24
    u32 null;               // 0x28
};

BW_DATA(Options*, g_options_list, 0x006CDFD4);

BW_DATA(u8*, bw_game_speed, 0x0059bb6c);
//BW_DATA(u8*, bw_game_speed, 0x006cdfd4);

typedef char ForceNames[4][30];

BW_DATA(ForceNames*, bw_force_names, 0x0058d5bc);

inline const StormStdCall<b32(
    char* gamename, char* password, char* description, u32 category_bits, void* ldesc, u32 ld_size,
    void* initdata, u32 initdata_size, u32 max_players, char* player_name, char* player_description, DWORD* playerid
)>
    SNetCreateLadderGame{138};

typedef BOOL(__stdcall* MapPrepPtr)(const char* map_filename, char* param2, u32 num_param);
inline MapPrepPtr map_prep = (MapPrepPtr)0x004bf5d0;

typedef BOOL(__stdcall* CreateLadderGame)(GameData* game_data, const char* game_password);
inline CreateLadderGame create_ladder_game = (CreateLadderGame)0x004d3910;

struct DirEntryMap {
    DirEntryMap* prev;
    DirEntryMap* next;
    char filename[65];
    char map_name[32];
    char map_description[316];
    char total_players_str[35];
    char ai_players_str[35];
    char human_players_str[35];
    char map_size[35];
    char map_tileset[35];
    u32 list_index;
    b32 map_loaded;
    u32 map_format_ok;
    u8 game_state;
    u8 byte1;
    u8 byte2;
    u8 byte3;
    u32 int4;
    u8 selection_type;
    char full_path[260]; // includes filename
    char full_filename[260];
    u8 byte4;
    u16 map_width_tiles;
    u16 map_height_tiles;
    u16 tileset_id;
    u8 byte_5;
    u8 max_players;
    u8 computer_players;
    u8 human_players;
    char char1[16];
    GameData game_data;
    u8 force_map[8];
    char char2[40];
    u8 byte6;
    u8 byte7;
    void* unknown_ptr;
    u8 byte8;
    u8 byte9;
};

typedef BOOL(__stdcall* MapLoadInfo)();
inline MapLoadInfo map_load_info_proc = (MapLoadInfo)0x004a7740; // pass via eax
inline BOOL map_load_info(DirEntryMap* map) {
    __asm {
        mov eax, map
    }
	return map_load_info_proc();
}

typedef void(__stdcall* LS_DIR)(u8 flags);
inline LS_DIR ls_dir = (LS_DIR)0x004a7050;
inline void list_dir(const char* root_dir) {
    __asm {
		mov edi, root_dir
	}
    ls_dir(28);
}

typedef BOOL(__stdcall* CreateGameMapSelectProc)();
inline CreateGameMapSelectProc create_game_map_select_proc = (CreateGameMapSelectProc)0x004a8050;
    //    const char* game_name, const char* game_password, u32 game_mode, u32 unknown, const char* current_working_directory
//);
//inline CreateGameMapSelectProc create_game_map_select_proc = (CreateGameMapSelectProc)0x004a8050;
inline u32 create_game(
    const char* game_name, const char* game_password, u32 game_mode, u32 unknown, const char* current_working_directory,
    DirEntryMap* selected_entry
) {
    BOOL retval;
    __asm {
        mov eax, selected_entry
        push current_working_directory
        push 0x00000000
        push game_mode
        push game_password
        push game_name
        call create_game_map_select_proc
        mov retval, eax
    }
    return retval;
}


// 1505ad70


struct ProviderInfo {
    ProviderInfo* prev;
    ProviderInfo* next;
    char filename[260];
    u32 provider_index;
    u32 provider_id; // 'BNET'
    char name[128];
    char description[128];
    Caps caps;
};

struct ProviderListEntry {
    ProviderInfo provider_info;
    u16 size; // sizeof(ProviderInfo)
    void* list_end;
    u16 unknown; // 0x6f6d
};

template <typename T>
struct StormList {
    T* last;
    T* first;

    struct Iterator {
        T* m_current;

        Iterator(T* ptr) : m_current{ptr} {}

        T& operator*() const { return *m_current; }
        T* operator->() { return m_current; }

        Iterator& operator++() {
            m_current = m_current->next;
            return *this;
        }

        Iterator operator++(int) {
            const auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const Iterator&) const = default;
    };

    Iterator begin() const { return {first}; };
    Iterator end() const { return {last->next}; };
};

#define BW_REF(Type, Name, Address) inline auto& Name = *reinterpret_cast<Type*>(Address);
BW_REF(StormList<ProviderInfo>, g_providers, 0x1505ad6c);
BW_REF(u32, g_provider_id, 0x0059688c);
BW_REF(ProviderInfo*, g_current_provider, 0x1505e62c);