#pragma once
#include <sys/time.h>

#define SEC_TO_MS(sec) ((sec) * 1000LL)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec) * 1000000LL)

#define PRINT_EXECUTION(LABEL) printf(LABEL " took: %.3fms\n", stop_timer())

static struct timeval t_start, t_stop;

static void start_timer() {
    gettimeofday(&t_start, (void *) 0);
} 

static double stop_timer() {
    gettimeofday(&t_stop, (void *) 0);
    return SEC_TO_MS(t_stop.tv_sec - t_start.tv_sec) + (double)(t_stop.tv_usec - t_start.tv_usec) / 1000;
}
