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
#include "engine/utils/logging.h"
#include "network.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>

#include <glad/glad.h>

RMultiplayerClient::RMultiplayerClient(char *server_address, char *server_port,
                                       ECS &ecs) {
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
  }

  this->server_address.sin_addr.s_addr = inet_addr(server_address);
  this->server_address.sin_port = htons(atoi(server_port));

  INFO("Connecting to server\n");
  if (connect(this->tcp_handle, (const struct sockaddr *)&this->server_address,
              sizeof(sockaddr_in)) < 0) {
    ERROR("connect failed");
    exit(EXIT_FAILURE);
  }

  INFO("Sending init message\n");
  client_request message;
  message.port = port;
  if (send(this->tcp_handle, &message, sizeof(client_request), 0) < 0) {
    ERROR("failed to request connection");
    exit(EXIT_FAILURE);
  }

  client_init buffer;

  INFO("Aquiering game state\n");
  if (recv(this->tcp_handle, (char *)&buffer, sizeof(client_init), 0) < 0) {
    ERROR("failed to get initial state");
    exit(EXIT_FAILURE);
  }

  this->player_id = buffer.id;

  // for (int i = 0; i < buffer.num_players; i++) {
  //   auto online_player = ecs.create_entity();
  //   ecs.add_component(online_player, COnline{.id = i});
  //   auto pos = glm::vec3(0);
  //   ecs.add_component(online_player, CTranslation{.pos = pos});
  // }

  INFO("id: %d\n", buffer.id);
}

// SGetMessage::SGetMessage() {
//   queries[0] = Query(COnline::get_id(), CTranslation::get_id());
//   query_count = 1;
// }
//
// void add_new_player(ECS &ecs, RMultiplayerClient *client) {
//   client_position buffer;
//   if (recv(client->udp_handle, &buffer, sizeof(client_position), 0) < 0) {
//     return;
//   }
//
//   auto new_player = ecs.create_entity();
//   ecs.add_component(new_player, COnline{.id = buffer.id});
//   ecs.add_component(new_player,
//                     CTranslation{.pos = glm::vec3(buffer.x, 0, buffer.z),
//                                  .rot = glm::quat(1, 0, 0, 0),
//                                  .scale = glm::vec3(1)});
//   ecs.add_component(new_player, CVelocity{.vel = glm::vec3(0)});
// }
//
// void get_position(ECS &ecs, RMultiplayerClient *client, EntityArray *entities) {
//   number_of_players num;
//   if (recvfrom(client->udp_handle, (char *)&num, sizeof(number_of_players),
//                MSG_WAITALL, nullptr, nullptr) < 0) {
//     return;
//   }
//
//   Iterator it = {.next = 0};
//   Entity e;
//
//   for (int i = 0; i < num; i++) {
//     client_position buffer;
//     if (recvfrom(client->udp_handle, (char *)&buffer, sizeof(client_position),
//                  MSG_WAITALL, nullptr, nullptr) < 0) {
//       return;
//     }
//
//     while (entities->next(it, e)) {
//       CTranslation &translation = ecs.get_component<CTranslation>(e);
//       int id = ecs.get_component<COnline>(e).id;
//
//       if (buffer.id == id) {
//         translation.pos.x = buffer.x;
//         translation.pos.z = buffer.z;
//         // this->players[i].rot = buffer[i].rot;
//       }
//     }
//   }
// }
//
// void SGetMessage::update(ECS &ecs) {
//   message_type type;
//   auto client = ecs.get_resource<RMultiplayerClient>();
//   while (recvfrom(client->udp_handle, (char *)&type, sizeof(message_type),
//                   MSG_WAITALL, nullptr, nullptr) > 0) {
//     switch (type) {
//     case 0:
//       DEBUG("New player");
//       add_new_player(ecs, client);
//       break;
//     case 1: {
//       DEBUG("Get position");
//       auto entities = get_query(0)->get_entities();
//       get_position(ecs, client, entities);
//       break;
//     }
//     default:
//       ERROR("Unknown message");
//     }
//   }
// }
//
// SSendOnlinePosition::SSendOnlinePosition() {
//   queries[0] = Query(CLocal::get_id(), CTranslation::get_id());
//   query_count = 1;
// }
//
// void SSendOnlinePosition::update(ECS &ecs) {
//   auto client = ecs.get_resource<RMultiplayerClient>();
//   auto entities = get_query(0)->get_entities();
//   Iterator it = {.next = 0};
//   Entity e;
//   while (entities->next(it, e)) {
//     CTranslation &translation = ecs.get_component<CTranslation>(e);
//     struct {
//       message_type type;
//       client_position pos;
//     } message;
//     message.type = 1;
//     message.pos.id = client->player_id;
//     message.pos.x = translation.pos.x;
//     message.pos.z = translation.pos.z;
//     message.pos.rot = 0;
//
//     sendto(client->udp_handle, (const char *)&message, sizeof(message),
//            MSG_CONFIRM, (const struct sockaddr *)&client->server_address,
//            sizeof(client->server_address));
//   }
// }
