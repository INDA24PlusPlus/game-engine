#include "shortestpath.h"



vpl dirs2 = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}}; 

string pathfind(vvl &grid, pl start, pl end){ // kanske inte super optimal
    ll n = grid.size();
    ll m = grid[0].size();
    vvl dist(n, vl(m, 1e9));
    vvl par(n, vl(m, -1));
    priority_queue<tuple<double, ll, ll, ll>> pq;
    pq.push({-(abs(start.first-end.first) + abs(start.second-end.second)), start.first, start.second, -1});
    while(!pq.empty()){
        auto [d, x, y, dir] = pq.top();
        pq.pop();
        if(1e9 != dist[x][y]) continue;
        d = -d;
        cout << x << " " << y << " " << d << endl;
        par[x][y] = dir;
        d = d-abs(x-end.first)-abs(y-end.second);
        dist[x][y] = d;
        if(x == end.first && y == end.second) break;
        for(ll i = 0; i < 4; i++){
            ll nx = x + dirs2[i].first;
            ll ny = y + dirs2[i].second;
            if(nx < 0 || nx >= n || ny < 0 || ny >= m) continue;
            if(grid[nx][ny] == -1) continue;
            pq.push({-(1+d+abs(nx-end.first)+abs(ny-end.second)), nx, ny, i});
        }
    }
    pl cur = end;
    string res = "";
    while(par[cur.first][cur.second] != -1){
        cout << cur.first << " " << cur.second << endl;
        if(par[cur.first][cur.second] == 0) res = ">" + res;
        if(par[cur.first][cur.second] == 1) res = "v" + res;
        if(par[cur.first][cur.second] == 2) res = "<" + res;
        if(par[cur.first][cur.second] == 3) res = "^" + res;
        cur = {cur.first-dirs2[par[cur.first][cur.second]].first, cur.second-dirs2[par[cur.first][cur.second]].second};
    }
    return res;
            

}

