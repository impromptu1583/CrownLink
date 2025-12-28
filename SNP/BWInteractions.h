#pragma once
#include "Common.h"
#include "../NetShared/StormTypes.h"

#define BW_REF(type, name, offset) inline type& name = *(type*)offset
#define BW_ARRAY(type, size, name, offset) inline std::array<type, size>& name = *(std::array<type, size>*)offset

// Important notes about this list:
// If providers have not been listed yet it points to itself
// Check g_providers_listed to see if they have been listed
// The final next* goes back to the list head pointer as well 0x1505ad6c
// If you just follow next* blindly you'll have an endless loop
BW_REF(ProviderInfo*, g_providers_list, 0x1505ad6c);
BW_REF(b32, g_providers_listed, 0x1505e630);  // does this change when enum is called?

BW_REF(ProviderInfo*, g_current_provider, 0x1505e62c);
inline TurnsPerSecond g_turns_per_second = TurnsPerSecond::Standard;
inline ClientInfo g_client_info{};