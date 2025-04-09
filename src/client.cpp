#include "client.hpp"
#include "network.hpp"
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>

Client::Client(char *client_port, char *server_address, char *server_port) {
  printf("client on port %s connecting to server on %s and port %s\n",
         client_port, server_address, server_port);
  // Creating socket file descriptor
  if ((this->tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Creating socket file descriptor
  if ((this->udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Filling server information
  this->address.sin_family = AF_INET; // IPv4
  this->address.sin_addr.s_addr = INADDR_ANY;
  this->address.sin_port = htons(atoi(client_port));

  // Bind the socket with the server address
  if (bind(this->udp, (const struct sockaddr *)&this->address,
           sizeof(this->address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  this->address.sin_addr.s_addr = inet_addr(server_address);
  this->address.sin_port = htons(atoi(server_port));

  printf("Connecting to server\n");
  if (connect(this->tcp, (const struct sockaddr *)&this->address,
              sizeof(sockaddr_in)) < 0) {
    perror("connect failed");
    exit(EXIT_FAILURE);
  }

  printf("Sending init message\n");
  client_request message;
  message.port = atoi(client_port);
  if (send(this->tcp, &message, sizeof(client_request), 0) < 0) {
    perror("failed to request connection");
    exit(EXIT_FAILURE);
  }

  client_init buffer;

  printf("Aquiering game state\n");
  if (recv(this->tcp, (char *)&buffer, sizeof(client_init), 0) < 0) {
    perror("failed to get initial state");
    exit(EXIT_FAILURE);
  }

  this->id = buffer.id;
  this->seed = buffer.seed;

  for (int i = 0; i < NUM_PLAYERS; i++) {
    this->players[i].x = buffer.players[i].x;
    this->players[i].y = buffer.players[i].y;
    this->players[i].rot = buffer.players[i].rot;
  }

  printf("id: %d\n", buffer.id);
}

void Client::get_positions() {
  client_position_update buffer[NUM_PLAYERS];
  sockaddr_in serveraddr;
  socklen_t len;
  int n;

  while (true) {
    n = recvfrom(this->udp, (char *)&buffer, sizeof(client_position_update),
                 MSG_WAITALL, (struct sockaddr *)&serveraddr, &len);

    for (int i = 0; i < NUM_PLAYERS; i++) {
      this->players[i].x = buffer[i].x;
      this->players[i].y = buffer[i].y;
      this->players[i].rot = buffer[i].rot;
    }
  }
}

PlayerPosition Client::get_position(int id) { return players[id]; }

void Client::send_position(float x, float y, float rot) {
  client_position_update pos;
  pos.id = this->id;
  pos.x = x;
  pos.y = y;
  pos.rot = rot;

  sendto(this->udp, (const char *)&pos, sizeof(pos), MSG_CONFIRM,
         (const struct sockaddr *)&this->address, sizeof(this->address));
}

void Client::quit() {
  close(this->tcp);
  close(this->udp);
}
