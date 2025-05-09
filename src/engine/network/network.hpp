#pragma once
#include <sys/types.h>

// Send over UDP
using message_type = int;
using number_of_players = int;
using number_of_enemy = int;
using new_player_id = int;

typedef struct {
  int id;
  float x;
  float z;
  float rot;
} client_position;

typedef struct {
  int id;
  float x;
  float y;
  float z;
  float rot;
} enemy_position;

// Send over TCP
typedef struct {
  int port;
} client_request;
