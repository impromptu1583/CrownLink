#pragma once
#include "../types.h"
#include "../NetShared/StormTypes.h"

namespace snp {

void packet_parser(const GamePacket* game_packet);
bool set_snp_turns_per_second(TurnsPerSecond turns_per_second);

TurnsPerSecond get_snp_turns_per_second();

// Records which provider entry storm bound us to; must be called from SnpBind before spi_initialize runs
void record_bound_provider(u32 index);

extern NetFunctions g_spi_functions;

};  // namespace snp
