#pragma once
#include "engine/ecs/component.hpp"
#include "engine/ecs/resource.hpp"
#include "engine/ecs/system.hpp"
#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_PLAYERS 8
#define NUM_ENEMY 10

#define ADD_PLAYER_MESSAGE 0
#define UPDATE_PLAYER_POSITION_MESSAGE 1
#define ADD_ENEMY_MESSAGE 2
#define UPDATE_ENEMY_POSITION_MESSAGE 3

class RServer : public Resource<RServer> {
public:
  RServer(char *port);
  sockaddr_in address;
  int tcp_handle;
  int udp_handle;
};

class CClient : public Component<CClient> {
public:
  int id;
  float x;
  float z;
  float rot;
  sockaddr_in address;
  int socket;
};

class SSendPositions : public System<SSendPositions> {
public:
  SSendPositions();
  void update(ECS &ecs);
};

class SGetPositions : public System<SGetPositions> {
public:
  SGetPositions();
  void update(ECS &ecs);
};

class SAcceptClients : public System<SAcceptClients> {
public:
  SAcceptClients();
  void update(ECS &ecs);
};

class SSendEnemyPositions : public System<SSendEnemyPositions> {
public:
  SSendEnemyPositions();
  void update(ECS &ecs);
};
