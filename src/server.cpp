#include "server.hpp"
#include "network.hpp"
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

Server::Server(char *port) {
  this->current_players = 0;
  this->seed = 0;
  this->running = true;
  for (int i = 0; i < NUM_PLAYERS; i++) {
    memset(&this->clients[i], 0, sizeof(client));
  }

  printf("server on port %s\n", port);
  // Creating socket file descriptor for tcp
  if ((this->tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Creating socket file descriptor for udp
  if ((this->udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Filling server information
  this->address.sin_family = AF_INET; // IPv4
  this->address.sin_addr.s_addr = INADDR_ANY;
  this->address.sin_port = htons(atoi(port));

  // Bind the socket with the server address
  if (bind(this->tcp, (const struct sockaddr *)&this->address,
           sizeof(this->address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  // Bind the socket with the server address
  if (bind(this->udp, (const struct sockaddr *)&this->address,
           sizeof(this->address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
}

void Server::run() {
  printf("Starting server\n");
  std::thread send_thread(&Server::send_positions, this);
  std::thread get_thread(&Server::get_positions, this);
  std::thread accept_thread(&Server::accept_client, this);

  while (running) {
    char buf[256];
    scanf("%s", buf);
    if (strcmp(buf, "quit")) {
      this->running = false;
    }
  }

  close(this->udp);
  close(this->tcp);
}

void Server::get_positions() {
  client_position_update buffer;
  int n;

  while (this->running) {
    n = recvfrom(this->udp, (char *)&buffer, sizeof(client_position_update),
                 MSG_WAITALL, nullptr, nullptr);

    printf("(%f, %f, %f)\n", buffer.x, buffer.y, buffer.rot);

    this->clients[buffer.id].x = buffer.x;
    this->clients[buffer.id].y = buffer.y;
    this->clients[buffer.id].rot = buffer.rot;
  }
}

void Server::send_positions() {
  client_position_update pos[NUM_PLAYERS];

  while (running) {
    for (int i = 0; i < this->current_players; i++) {
      pos[i].id = i;
      pos[i].x = this->clients[i].x;
      pos[i].y = this->clients[i].y;
      pos[i].rot = this->clients[i].rot;
    }

    for (int i = 0; i < this->current_players; i++) {
      sendto(this->udp, (const char *)&pos, sizeof(pos), MSG_CONFIRM,
             (const struct sockaddr *)&this->clients[i].address,
             sizeof(this->clients[i].address));
    }

    sleep(1);
  }
}

void Server::accept_client() {
  listen(this->tcp, 20);

  while (running) {
    sockaddr_in address;
    unsigned int n;

    printf("Listening for client %d\n", this->current_players);

    this->clients[this->current_players].socket =
        accept(this->tcp, (struct sockaddr *)&address, &n);

    if (this->clients[this->current_players].socket < 0) {
      perror("failed to accept client");
      exit(EXIT_FAILURE);
    }

    this->clients[this->current_players].address = address;

    printf("Client accepted\n");

    // TODO: set client starting point
    //
    client_request request;

    if (recv(this->clients[this->current_players].socket, (char *)&request, sizeof(client_request), 0) < 0) {
      perror("failed to get client port");
      exit(EXIT_FAILURE);
    }

    this->clients[this->current_players].address.sin_port = htons(request.port);

    client_init buffer;
    buffer.id = this->current_players;
    buffer.seed = this->seed;

    for (int i = 0; i <= this->current_players; i++) {
      buffer.players[i].x = this->clients[i].x;
      buffer.players[i].y = this->clients[i].y;
      buffer.players[i].rot = this->clients[i].rot;
    }

    if (send(this->clients[this->current_players].socket, &buffer,
             sizeof(client_init), 0) < 0) {
      perror("failed to send state to client");
      exit(EXIT_FAILURE);
    }

    printf("New player with id: %d\n", this->current_players);

    this->current_players++;
  }
}
