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