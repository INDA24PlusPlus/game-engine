#pragma once
#include "engine/ecs/component.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/ecs/resource.hpp"
#include "network.hpp"
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

typedef struct {
  float x;
  float y;
  float rot;
} PlayerPosition;

class Client {
public:
  Client();
  int get_id();
  PlayerPosition get_position(int id);
  void connect_to_server(char *server_address, char *server_port);
  void send_position(float x, float y, float rot);
  void quit();

private:
  void get_positions();

  PlayerPosition players[NUM_PLAYERS];
  std::thread get_thread;
};

class RMultiplayerClient : public Resource<RMultiplayerClient> {
public:
  RMultiplayerClient(char *server_address, char *server_port, ECS &ecs);

  int player_id;
  sockaddr_in server_address;
  int udp_handle;
  int tcp_handle;
  std::thread get_thread;
};

class COnline : public Component<COnline> {
public:
  int id;
};

class SGetMessage : public System<SGetMessage> {
public:
  SGetMessage();
  void update(ECS &ecs);
};

class SSendOnlinePosition : public System<SSendOnlinePosition> {
public:
  SSendOnlinePosition();
  void update(ECS &ecs);
};
