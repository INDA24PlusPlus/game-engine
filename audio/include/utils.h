#pragma once
#include <assert.h>
#include <stdint.h>
#include <sound.h>

#define TO_STR(x) #x
#define TO_STR_VALUE(x) TO_STR(x)

#define CLAMP_VALUE_MAX_MIN(VALUE, MIN, MAX) { \
    if (MAX < VALUE) VALUE = MAX; \
    else if (VALUE < MIN) VALUE = MIN; \
}
#define CLAMP_VALUE_BY_TYPE(VALUE, TYPE) CLAMP_VALUE_MAX_MIN(VALUE, TYPE##_MIN, TYPE##_MAX)

#ifdef _pcms16le
#define CLAMP_VALUE(VALUE) CLAMP_VALUE_BY_TYPE(VALUE, INT16)
#else
static_assert(0, "CLAMP_VALUE not implemented");
#endif

#define sizeof_member(type, member) (sizeof(((type *)0)->member))
