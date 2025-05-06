#pragma once

#include "types.h"
#include <bitset>

const u32 MAX_COMPONENTS = 512;
using Signature = std::bitset<MAX_COMPONENTS>;

Signature set_signature(Signature signature, u32 component_id);

Signature remove_signature(Signature signature, u32 component_id);
