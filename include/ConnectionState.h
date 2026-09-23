#pragma once

#include <storm/types.hpp>

enum class ConnectionState : u32 {
    Disconnected,
    Connecting,
    Failed,
    Standard,
    Relay,
    Radmin,
    BadVersion,
};