#pragma once
#include <sys/types.h>

#define PORT 1337
#define MAXLINE 1024
#define NUM_PLAYERS 2

typedef struct {
  float x;
  float y;
  float rot;
} client_position;

typedef struct {
  int id;
  float x;
  float y;
  float rot;
} client_position_update;

typedef struct {
  int id;
  u_int64_t seed;
  client_position players[NUM_PLAYERS];
} client_init;

typedef struct {
    int port;
} client_request;
