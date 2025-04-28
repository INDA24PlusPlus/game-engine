#pragma once
#include <sys/time.h>

#define SEC_TO_MS(sec) ((sec) * 1000LL)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec) * 1000000LL)

#define PRINT_EXECUTION(LABEL) printf(LABEL " took: %.3fms\n", stop_timer())

void start_timer();
double stop_timer();
