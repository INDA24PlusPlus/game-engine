#include "utils/logging.h"

void test() {
    Logger logger = Logger("other");
    LOGGER_INFO(logger, "Hello from test!");
}
