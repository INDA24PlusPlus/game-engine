#ifndef MAP_H
#define MAP_H

#include "union_find.h"
#include <vector>
#include <tuple>
#include <utility>
#include <queue>
#include <cmath>
#include <random>

class Map {
private:
    const std::vector<std::pair<long long, long long>> dirs = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    std::mt19937 rng;
    bool check_room(long long x1, long long y1, long long x2, long long y2);
    void place_room(long long x1, long long y1, long long x2, long long y2, long long val);
    void generate_rooms(long long amount_rooms, long long min_size, long long max_size);
    void generate_paths();

public:
    std::vector<std::vector<long long>> grid;
    std::vector<std::tuple<long long, long long, long long, long long>> rooms;
    Map(long long size, long long amount_rooms, long long min_size, long long max_size, long long seed);
    void generate(long long amount_rooms, long long min_size, long long max_size);
    void print();
    ~Map() = default;
};

#endif