#include "types.h"
#include <cstdlib>
#include <cstdio>


void assert(bool condition, const char* message) {
    if (!condition) {
        printf("Assertion failed: %s\n", message);
        exit(1);
    }
}
