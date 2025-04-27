#include "map.h"
#include <iostream>
#include <cmath>


bool Map::check_room(ll x1, ll y1, ll x2, ll y2) {
    if(x1 < 0 || y1 < 0 || x2 >= grid.size() || y2 >= grid[0].size()) return false;
    for(ll i = x1; i <= x2; i++){
        for(ll j = y1; j <= y2; j++){
            if(grid[i][j]!=-1) return false;
        }
    }
    return true;
}

void Map::place_room(ll x1, ll y1, ll x2, ll y2, ll val){
    for(ll i = x1; i <= x2; i++){
        for(ll j = y1; j <= y2; j++){
            grid[i][j] = val;
        }
    }
}

vpl dirs = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

void Map::generate_rooms(ll amount_rooms, ll min_size, ll max_size){
    while(rooms.size() < amount_rooms){
        ll x1 = rng() % grid.size();
        ll y1 = rng() % grid[0].size();
        ll x2 = x1 + (rng() % (max_size - min_size + 1)) + min_size;
        ll y2 = y1 + (rng() % (max_size - min_size + 1)) + min_size;
        
        if(!check_room(x1-2, y1-2, x2+2, y2+2)) continue;
        place_room(x1, y1, x2, y2, rooms.size());
        rooms.pb(std::make_tuple(x1, y1, x2, y2));
    }

    for(ll i = 0; i < rooms.size()*100; i++){
        ll room = rng() % rooms.size();
        ll x1, x2, y1, y2;
        std::tie(x1, y1, x2, y2) = rooms[room];
        pl dir;
        if(rng()%2) dir = {(x1+x2)/2<grid.size()/2?1:-1, 0};
        else dir = {0, (y1+y2)/2<grid.size()/2?1:-1};
        place_room(x1, y1, x2, y2, -1);
        if(check_room(x1+dir.F-2, y1+dir.S-2, x2+dir.F+2, y2+dir.S+2)){
            place_room(x1+dir.F, y1+dir.S, x2+dir.F, y2+dir.S, room);
            rooms[room] = {x1+dir.F, y1+dir.S, x2+dir.F, y2+dir.S};
        }else{
            place_room(x1, y1, x2, y2, room);
        }
    }
}

void Map::generate_paths() {
    vpl rooms_pos;
    fo(i, rooms.size()) {
        ll x1, y1, x2, y2;
        std::tie(x1, y1, x2, y2) = rooms[i];
        rooms_pos.pb({(rng()%(x2-x1+1))+x1, (rng()%(y2-y1+1))+y1});
    }
    std::priority_queue<std::tuple<double, ll, ll>> pq;
    for(ll i = 0; i < rooms.size(); i++){
        for(ll j = i+1; j < rooms.size(); j++){
            double dist = sqrt((rooms_pos[i].F-rooms_pos[j].F)*(rooms_pos[i].F-rooms_pos[j].F) + (rooms_pos[i].S-rooms_pos[j].S)*(rooms_pos[i].S-rooms_pos[j].S));
            pq.push({-dist, i, j});
        }
    }
    UnionFind uf(rooms.size());
    vpl edges;
    while(!pq.empty()){
        auto [dist, i, j] = pq.top(); pq.pop();
        if(uf.find(i) != uf.find(j)){
            uf.unite(i, j);
            edges.pb({i, j});
        }
    }

    for(auto [i, j] : edges){
        ll x1, y1, x2, y2;
        std::tie(x1, y1) = rooms_pos[i];
        std::tie(x2, y2) = rooms_pos[j];
        while(x1 != x2 || y1 != y2){
            if(rng()%2){
                if(x1 < x2) x1++;
                else if(x1 > x2) x1--;
            }else{
                if(y1 < y2) y1++;
                else if(y1 > y2) y1--;
            }
            if(grid[x1][y1] == -1) grid[x1][y1] = -2;
        }
    }
}

Map::Map(ll size, ll amount_rooms, ll min_size, ll max_size, ll seed){
    rng.seed(seed);
    grid.assign(size, vl(size, -1));
    generate_rooms(amount_rooms, min_size, max_size);
    generate_paths();
}

void Map::print(){
    std::cout << "Rummen: ";
    fo(i, rooms.size()) std::cout << i << " ";
    std::cout << std::endl;
    fo(i, grid.size()) {
        fo(j, grid[i].size()) {
            if(grid[i][j] == -1) std::cout << " ";
            else if(grid[i][j] == -2) std::cout << "#";
            else std::cout << char(grid[i][j]+'a');
        }
        std::cout << std::endl;
    }
}
