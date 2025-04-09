#pragma once
// Server side implementation of UDP client-server model
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLINE 1024
#define NUM_PLAYERS 2

struct client {
  float x;
  float y;
  float rot;
  sockaddr_in address;
  int socket;
};

class Server {
public:
  Server(char *port);
  void run();

private:
  void send_positions();
  void get_positions();
  void accept_client();

  bool running;
  int current_players;
  u_int64_t seed;
  client clients[NUM_PLAYERS];
  sockaddr_in address;
  int udp;
  int tcp;
};
