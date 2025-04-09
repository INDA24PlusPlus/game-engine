#pragma once

#include "types.h"

using Signature = u32;

Signature set_signature(Signature signature, u32 component_id);

Signature remove_signature(Signature signature, u32 component_id);
