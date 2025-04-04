#include "imgui.h"
#include <iostream>

#include "utils/logging.h"

void test();

int main() {
    Logger main_logger = Logger("Main");

    LOGGER_INFO(main_logger, "Hello, World");
    LOGGER_DEBUG(main_logger, "Hello, World");
    LOGGER_WARN(main_logger, "Hello, World");
    LOGGER_ERROR(main_logger, "Hello, World");
    test();
     
}
