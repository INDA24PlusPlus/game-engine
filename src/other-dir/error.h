#include <stdio.h>

void name() {
    char name[10];
    fgets(name, 10, stdin);

    printf("Hello %s", name);
}
