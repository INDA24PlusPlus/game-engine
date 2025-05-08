#include "../ecs/ecs.hpp"
#include "../ecs/resource.hpp"
#include "server.hpp"

int main(int argc, char *argv[]) {
  ECS ecs = ECS();
  ecs.register_resource(new RServer(argv[1]));

  ecs.register_component<CClient>();

  auto accept_client_system = ecs.register_system<SAcceptClients>();
  // auto send_system = ecs.register_system<SSendPositions>();
  // auto get_system = ecs.register_system<SGetPositions>();

  while (true) {
    accept_client_system->update(ecs);
    // send_system->update(ecs);
    // get_system->update(ecs);
  }
}
