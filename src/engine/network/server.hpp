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

#define NUM_PLAYERS 2

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
