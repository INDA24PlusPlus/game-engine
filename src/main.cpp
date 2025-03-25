#include "imgui.h"
#include <stdio.h>

#define MAX_NAME_LENGTH 10

// clang-tidy test
int main() {
    char name[5];
    
    fgets(&name, MAX_NAME_LENGTH, stdin);

    printf("Hello there %s\n", name);
}
