#pragma once
#include <sys/types.h>

#define PORT 1337
#define MAXLINE 1024
#define NUM_PLAYERS 2

// Send over UDP
using message_type = int;
using number_of_players = int;
using new_player_id = int;

typedef struct {
  int id;
  float x;
  float z;
  float rot;
} client_position;

// Send over TCP
typedef struct {
    int port;
} client_request;
