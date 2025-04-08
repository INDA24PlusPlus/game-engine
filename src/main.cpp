#include <iostream>

#include "imgui.h"
#include "utils/logging.h"

void test();

int main() {
    INFO("Hello, World");
    DEBUG("Hello, World");
    WARN("Hello, World");
    ERROR("Hello, World");
    test();
}
