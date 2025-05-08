#include "client.hpp"
#include "../../game/components.h"
#include "../../game/player.h"
#include "../ecs/component.hpp"
#include "../ecs/ecs.hpp"
#include "../ecs/entity.hpp"
#include "../ecs/entityarray.hpp"
#include "../ecs/resource.hpp"
#include "../ecs/signature.hpp"
#include "../ecs/system.hpp"
#include "engine/network/server.hpp"
#include "engine/scene/Node.h"
#include "engine/utils/logging.h"
#include "network.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

#include "game/state.h"
#include <glad/glad.h>

RMultiplayerClient::RMultiplayerClient(char *server_address, char *server_port,
                                       ECS &ecs) {
  auto scene = ecs.get_resource<RScene>();
  srand(time(0));
  // Creating socket file descriptor
  if ((this->tcp_handle = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERROR("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Creating socket file descriptor
  if ((this->udp_handle = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    ERROR("socket creation failed");
    exit(EXIT_FAILURE);
  }

  if (fcntl(udp_handle, F_SETFL, O_NONBLOCK) < 0) {
    ERROR("set udp as non-blocking");
    exit(EXIT_FAILURE);
  }

  int port = (rand() % 5000 + 20000);

  // Filling server information
  this->server_address.sin_family = AF_INET; // IPv4
  this->server_address.sin_addr.s_addr = INADDR_ANY;
  this->server_address.sin_port = htons(port);

  printf("binding on port: %d\n", port);
  // Bind the socket with the server address
  while (bind(this->udp_handle, (const struct sockaddr *)&this->server_address,
              sizeof(this->server_address)) < 0) {
    port = (rand() % 5000 + 20000);
    printf("binding on port: %d", port);
    this->server_address.sin_port = htons(port);
  }

  this->server_address.sin_addr.s_addr = inet_addr(server_address);
  this->server_address.sin_port = htons(atoi(server_port));

  INFO("Connecting to server");
  if (connect(this->tcp_handle, (const struct sockaddr *)&this->server_address,
              sizeof(sockaddr_in)) < 0) {
    ERROR("connect failed");
    exit(EXIT_FAILURE);
  }

  INFO("Sending init message");
  client_request message;
  message.port = port;
  if (send(this->tcp_handle, &message, sizeof(client_request), 0) < 0) {
    ERROR("failed to request connection");
    exit(EXIT_FAILURE);
  }

  struct {
    new_player_id id;
    number_of_players num_players;
  } init_message;

  INFO("Aquiering game state");
  if (recv(this->tcp_handle, (char *)&init_message,
           sizeof(new_player_id) + sizeof(number_of_players), 0) < 0) {
    ERROR("failed to get initial state");
    exit(EXIT_FAILURE);
  }

  this->player_id = init_message.id;

  INFO("Adding other players");
  for (int i = 0; i < init_message.num_players; i++) {

    client_position player;
    recv(this->tcp_handle, (char *)&player, sizeof(client_position), 0);
    auto online_player = ecs.create_entity();
    ecs.add_component<COnline>(online_player, COnline{.id = player.id});
    auto pos = glm::vec3(player.x, 0, player.z);
    ecs.add_component<CTranslation>(
        online_player, CTranslation{.pos = pos,
                                    .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                    .scale = glm::vec3(1)});
    ecs.add_component<CVelocity>(online_player,
                                 CVelocity{.vel = glm::vec3(0.0f)});
    ecs.add_component<CPlayer>(online_player, CPlayer());
    ecs.add_component<CSpeed>(online_player, CSpeed());
    ecs.add_component<CHealth>(online_player, CHealth());
    auto player_prefab = scene->m_scene.prefab_by_name("Player");
    engine::NodeHandle player_node = scene->m_hierarchy.instantiate_prefab(
        scene->m_scene, player_prefab, engine::NodeHandle(0));
    ecs.add_component<CMesh>(online_player, CMesh(player_node));
  }

  printf("id: %d\n", init_message.id);
}

SGetMessage::SGetMessage() {
  queries[0] = Query(COnline::get_id(), CTranslation::get_id());
  query_count = 1;
}

void add_new_player(ECS &ecs, int *buffer) {
  auto scene = ecs.get_resource<RScene>();
  client_position *pos = (client_position *)(buffer + 1);

  auto new_player = ecs.create_entity();
  ecs.add_component<COnline>(new_player, COnline{.id = pos->id});
  auto position = glm::vec3(pos->x, 0, pos->z);
  ecs.add_component<CTranslation>(
      new_player, CTranslation{.pos = position,
                               .rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                               .scale = glm::vec3(1)});
  ecs.add_component<CVelocity>(new_player, CVelocity{.vel = glm::vec3(0.0f)});
  ecs.add_component<CPlayer>(new_player, CPlayer());
  ecs.add_component<CSpeed>(new_player, CSpeed());
  ecs.add_component<CHealth>(new_player, CHealth());
  auto player_prefab = scene->m_scene.prefab_by_name("Player");
  engine::NodeHandle player_node = scene->m_hierarchy.instantiate_prefab(
      scene->m_scene, player_prefab, engine::NodeHandle(0));
  ecs.add_component<CMesh>(new_player, CMesh(player_node));
}

void get_position(ECS &ecs, int *buffer, EntityArray *entities) {
  int num_players = buffer[1];
  Iterator it = {.next = 0};
  Entity e;

  for (int i = 0; i < num_players; i++) {
    client_position *pos =
        (client_position *)(buffer + 2 +
                            i * (sizeof(client_position) / sizeof(int)));

    while (entities->next(it, e)) {
      auto &translation = ecs.get_component<CTranslation>(e);
      int id = ecs.get_component<COnline>(e).id;

      if (pos->id == id) {
        translation.pos.x = pos->x;
        translation.pos.z = pos->z;
        // this->players[i].rot = buffer[i].rot;
      }
    }
  }
}

void SGetMessage::update(ECS &ecs) {
  int buffer[1000];
  auto client = ecs.get_resource<RMultiplayerClient>();
  while (recvfrom(client->udp_handle, (char *)&buffer, sizeof(buffer),
                  MSG_WAITALL, nullptr, nullptr) > 0) {
    switch (buffer[0]) {
    case 0:
      DEBUG("New player");
      add_new_player(ecs, buffer);
      break;
    case 1: {
      auto entities = get_query(0)->get_entities();
      get_position(ecs, buffer, entities);
      break;
    }
    default:
      ERROR("Unknown message");
      printf("%d", buffer[0]);
    }
  }
}

SSendOnlinePosition::SSendOnlinePosition() {
  queries[0] = Query(CLocal::get_id(), CTranslation::get_id());
  query_count = 1;
}

void SSendOnlinePosition::update(ECS &ecs) {
  auto client = ecs.get_resource<RMultiplayerClient>();
  auto entities = get_query(0)->get_entities();
  Iterator it = {.next = 0};
  Entity e;
  while (entities->next(it, e)) {
    CTranslation &translation = ecs.get_component<CTranslation>(e);
    client_position pos;
    pos.id = client->player_id;
    pos.x = translation.pos.x;
    pos.z = translation.pos.z;
    pos.rot = 0;

    sendto(client->udp_handle, (const char *)&pos, sizeof(client_position),
           MSG_CONFIRM, (const struct sockaddr *)&client->server_address,
           sizeof(client->server_address));
  }
}
