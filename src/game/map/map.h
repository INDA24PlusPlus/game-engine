#ifndef MAP_H
#define MAP_H

#include "../../definitions.h"
#include "union_find.h"
#include <vector>
#include <tuple>
#include <utility>
#include <queue>
#include <cmath>
#include <random>

class Map {
private:
    const vpl dirs = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    mt19937 rng;
    bool check_room(ll x1, ll y1, ll x2, ll y2);
    void place_room(ll x1, ll y1, ll x2, ll y2, ll val);
    void generate_rooms(ll amount_rooms, ll min_size, ll max_size);
    void generate_paths();

public:
    vvl grid;
    vector<tuple<ll, ll, ll, ll>> rooms;
    Map(ll size, ll amount_rooms, ll min_size, ll max_size, ll seed);
    void generate(ll amount_rooms, ll min_size, ll max_size);
    void print();
    ~Map() = default;
};

#endif