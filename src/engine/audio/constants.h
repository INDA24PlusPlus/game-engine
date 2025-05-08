#pragma once

#define SAMPLE_RATE 44100
#define CHANNELS 2

#define MAX_SOUND_BUF_SIZE 50 * 8 * 1024 // 50KB
#define SOUND_BUF_TYPE i16
#define PCMS16LE // remove this if SOUND_BUF_TYPE is no longer i16

#define CLAMP_VALUE_MAX_MIN(VALUE, MIN, MAX) { \
    if (MAX < VALUE) VALUE = MAX; \
    else if (VALUE < MIN) VALUE = MIN; \
}
#define CLAMP_VALUE_BY_TYPE(VALUE, TYPE) CLAMP_VALUE_MAX_MIN(VALUE, TYPE##_MIN, TYPE##_MAX)

#ifdef PCMS16LE
#define CLAMP_VALUE(VALUE) CLAMP_VALUE_BY_TYPE(VALUE, INT16)
#else
static_assert(0, "CLAMP_VALUE not implemented");
#endif
