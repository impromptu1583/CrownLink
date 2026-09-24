#pragma once
#include "../types.h"
#include "../NetShared/StormTypes.h"

#include "crownlink/provider_data.h"

namespace snp {

// provider_ids comes from the generated header; the ids must match the caps.dat entries that
// storm enumerates from caps.mpq, and this size is mirrored by CAPS_SIZE_BYTES in build_caps.py
static_assert(sizeof(Caps) == 36);

bool set_snp_turns_per_second(TurnsPerSecond turns_per_second);

TurnsPerSecond get_snp_turns_per_second();

// Records which provider entry storm bound us to; must be called from SnpBind before spi_initialize runs
void record_bound_provider(u32 index);

extern NetFunctions g_spi_functions;

};  // namespace snp
