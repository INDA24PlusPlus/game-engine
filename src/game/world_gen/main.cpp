#include "game/map/map.h"
#include "game/pathfind/shortestpath.h"

int main(){
    Map map(65, 15, 5, 10, 1337);
    string path = pathfind(map.grid, {37, 30}, {37, 23});
    map.grid[37][30] = 20;
    map.grid[37][23] = 21;
    map.print();
    cout << path << endl;
}

