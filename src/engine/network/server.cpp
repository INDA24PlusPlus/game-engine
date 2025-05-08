#include "server.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/system.hpp"
#include "engine/network/client.hpp"
#include "engine/utils/logging.h"
#include "network.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

RServer::RServer(char *port) {
  printf("server on port %s\n", port);
  // Creating socket file descriptor for tcp
  if ((this->tcp_handle = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERROR("socket creation failed");
    exit(EXIT_FAILURE);
  }

  if (fcntl(tcp_handle, F_SETFL, O_NONBLOCK) < 0) {
    perror("set tcp as non-blocking");
    exit(EXIT_FAILURE);
  }

  // Creating socket file descriptor for udp
  if ((this->udp_handle = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    ERROR("socket creation failed");
    exit(EXIT_FAILURE);
  }

  if (fcntl(udp_handle, F_SETFL, O_NONBLOCK) < 0) {
    perror("set udp as non-blocking");
    exit(EXIT_FAILURE);
  }

  // Filling server information
  this->address.sin_family = AF_INET; // IPv4
  this->address.sin_addr.s_addr = INADDR_ANY;
  this->address.sin_port = htons(atoi(port));

  // Bind the socket with the server address
  if (bind(this->tcp_handle, (const struct sockaddr *)&this->address,
           sizeof(this->address)) < 0) {
    ERROR("bind failed");
    exit(EXIT_FAILURE);
  }

  // Bind the socket with the server address
  if (bind(this->udp_handle, (const struct sockaddr *)&this->address,
           sizeof(this->address)) < 0) {
    ERROR("bind failed");
    exit(EXIT_FAILURE);
  }

  listen(tcp_handle, 20);
}

SGetPositions::SGetPositions() {
  queries[0] = Query(CClient::get_id());
  query_count = 1;
}

void SGetPositions::update(ECS &ecs) {
  client_position buffer;

  auto server = ecs.get_resource<RServer>();

  if (recvfrom(server->udp_handle, (char *)&buffer, sizeof(client_position),
               MSG_WAITALL, nullptr, nullptr) < 0) {
    return;
  }

  auto entities = get_query(0)->get_entities();
  Iterator it = {.next = 0};
  Entity e;
  while (entities->next(it, e)) {
    CClient &client = ecs.get_component<CClient>(e);

    if (buffer.id == client.id) {
      client.x = buffer.x;
      client.z = buffer.z;
    }
  }
}

SSendPositions::SSendPositions() {
  queries[0] = Query(CClient::get_id());
  queries[1] = Query(CClient::get_id());
  query_count = 2;
}

void SSendPositions::update(ECS &ecs) {
  auto server = ecs.get_resource<RServer>();
  auto entities = get_query(0)->get_entities();
  auto to_send = get_query(1)->get_entities();
  Iterator it = {.next = 0};
  Entity e;
  while (entities->next(it, e)) {
    auto client = ecs.get_component<CClient>(e);
    Iterator send_it = {.next = 0};
    Entity send;
    int i = 0;

    struct {
      message_type type;
      number_of_players num;
      client_position pos[NUM_PLAYERS];
    } message;

    message.type = 1;

    while (to_send->next(send_it, send)) {
      auto player_pos = ecs.get_component<CClient>(send);
      if (client.id == player_pos.id) {
        continue;
      }
      message.pos[i].id = player_pos.id;
      message.pos[i].x = player_pos.x;
      message.pos[i].z = player_pos.z;
      i++;
    }

    if (i == 0) {
      continue;
    }

    message.num = i;

    if (sendto(server->udp_handle, (const char *)&message,
               2 * sizeof(int) + sizeof(client_position) * i, MSG_CONFIRM,
               (const struct sockaddr *)&client.address,
               sizeof(client.address)) < 0) {
      ERROR("failed to send pos");
    }
  }
}

SAcceptClients::SAcceptClients() {
  queries[0] = Query(CClient::get_id());
  query_count = 1;
}

void SAcceptClients::update(ECS &ecs) {
  auto *server = ecs.get_resource<RServer>();

  auto client = CClient{
      .id = 0,
      .x = 0,
      .z = 0,
      .rot = 0,
      .address = 0,
      .socket = 0,
  };
  unsigned int n = sizeof(sockaddr_in);
  int socket;

  socket = accept(server->tcp_handle, (struct sockaddr *)&client.address, &n);

  if (socket == -1) {
    return;
  }

  client.socket = socket;

  if (client.socket < 0) {
    ERROR("failed to accept client");
    exit(EXIT_FAILURE);
  }

  INFO("Client accepted");

  // TODO: set client starting point
  client_request request;

  while (recv(client.socket, (char *)&request, sizeof(client_request), 0) < 0) {
  }

  client.address.sin_port = htons(request.port);

  int id = rand();

  struct {
    new_player_id id;
    number_of_players num_players;
    client_position pos[NUM_PLAYERS];
  } init_message;

  auto to_send = get_query(0)->get_entities();
  Iterator send_it = {.next = 0};
  Entity send_e;

  // Find unique id
  DEBUG("Find unique id");
  bool unique = false;
  while (!unique) {
    unique = true;
    while (to_send->next(send_it, send_e)) {
      auto send_id = ecs.get_component<CClient>(send_e).id;
      if (send_id == id) {
        unique = false;
      }
    }
  }

  printf("id: %d", id);
  init_message.id = id;
  client.id = id;
  send_it = {.next = 0};
  int i = 0;

  // Populate initial positions for other players
  while (to_send->next(send_it, send_e)) {
    auto player = ecs.get_component<CClient>(send_e);
    init_message.pos[i].id = player.id;
    init_message.pos[i].x = player.x;
    init_message.pos[i].z = player.z;
    init_message.pos[i].rot = player.rot;
    i++;
  }

  init_message.num_players = i;

  if (send(client.socket, &init_message,
           sizeof(new_player_id) + sizeof(number_of_players) +
               sizeof(client_position) * i,
           0) < 0) {
    ERROR("failed to send state to client");
    exit(EXIT_FAILURE);
  }

  send_it = {.next = 0};
  while (to_send->next(send_it, send_e)) {
    INFO("Inform about new player");
    auto client = ecs.get_component<CClient>(send_e);
    struct {
      message_type type;
      client_position pos;
    } message;

    message.type = 0;
    message.pos.id = id;
    message.pos.x = 0;
    message.pos.z = 0;
    message.pos.rot = 0;
    sendto(server->udp_handle, (const char *)&message,
           sizeof(message_type) + sizeof(client_position), MSG_CONFIRM,
           (const struct sockaddr *)&client.address, sizeof(client.address));
  }

  auto new_player = ecs.create_entity();
  ecs.add_component(new_player, client);

  INFO("Client created");
}
