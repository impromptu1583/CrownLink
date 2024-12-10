#pragma once
#include "../../shared_common.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

inline auto g_storm_dll = GetModuleHandle("storm.dll");

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

//
//inline HMODULE g_storm_module = GetModuleHandle("storm.dll");
//
//typedef int __stdcall SErrGetLastError_Func();
//inline SErrGetLastError_Func* SErrGetLastError =
//    reinterpret_cast<SErrGetLastError_Func*>(GetProcAddress(g_storm_module, MAKEINTRESOURCE(463)));
//
//
//typedef bool __stdcall SErrGetErrorStr_Func(DWORD ec, char* buffer, size_t buffersize);
//inline SErrGetErrorStr_Func* SErrGetErrorStr =
//    reinterpret_cast<SErrGetErrorStr_Func*>(GetProcAddress(g_storm_module, MAKEINTRESOURCE(462)));
//
//typedef int __stdcall SNetJoinGame_Func(
//    DWORD gameid, char* gamename, char* gamepassword, char* playername, char* playerdescription, DWORD* playerid
//);
//inline SNetJoinGame_Func* SNetJoinGame =
//    reinterpret_cast<SNetJoinGame_Func*>(GetProcAddress(g_storm_module, MAKEINTRESOURCE(462)));
//


//        if (snjg == NULL) {
//    auto hresult = GetLastError();
//    LPTSTR errorText = NULL;
//    FormatMessage(
//        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hresult, 0,
//        (LPTSTR)&errorText, 0, NULL
//    );
//
//    spdlog::info("Error: {}", errorText);
//    return false;
//}