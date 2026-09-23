#pragma once
#include "../types.h"
#include "../NetShared/StormTypes.h"

namespace snp {

// Provider ids as declared in the caps.dat entries of caps.mpq; array order matches the SnpQuery indices
inline constexpr u32 provider_ids[] = {'CNLK', 'CLDB'};

bool set_snp_turns_per_second(TurnsPerSecond turns_per_second);

TurnsPerSecond get_snp_turns_per_second();

// Records which provider entry storm bound us to; must be called from SnpBind before spi_initialize runs
void record_bound_provider(u32 index);

extern NetFunctions g_spi_functions;

};  // namespace snp
