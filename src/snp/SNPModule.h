#pragma once
#include "shared/types.h"
#include "shared/StormTypes.h"

#include "crownlink/provider_data.h"

namespace snp {

bool set_snp_turns_per_second(TurnsPerSecond turns_per_second);

TurnsPerSecond get_snp_turns_per_second();

// Records which provider entry storm bound us to. Must be called from SnpBind before spi_initialize runs
void record_bound_provider(u32 index);

extern NetFunctions g_spi_functions;

};  // namespace snp
