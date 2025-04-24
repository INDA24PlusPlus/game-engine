#include "server.hpp"
int main (int argc, char *argv[]) {
    Server server = Server(argv[1]);
    server.run();
    return 0;
}
