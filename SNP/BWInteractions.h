#pragma once
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

inline void set_provider_turns_per_second(TurnsPerSecond turns_per_second) {
    if (turns_per_second == TurnsPerSecond::CNLK) turns_per_second = TurnsPerSecond::Standard;
    if (turns_per_second == TurnsPerSecond::CLDB) turns_per_second = TurnsPerSecond::UltraLow;
    g_current_provider->caps.turns_per_second = turns_per_second;
}