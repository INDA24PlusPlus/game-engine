#pragma once
// Server side implementation of UDP client-server model
#include "network.hpp"
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
  float x;
  float y;
  float rot;
} PlayerPosition;

class Client {
public:
  Client(char *client_port, char *server_address, char *server_port);
  PlayerPosition get_position(int id);
  void send_position(float x, float y, float rot);
  void quit();

private:
  void get_positions();

  int id;
  u_int64_t seed;
  PlayerPosition players[NUM_PLAYERS];
  sockaddr_in address;
  int udp;
  int tcp;
};
