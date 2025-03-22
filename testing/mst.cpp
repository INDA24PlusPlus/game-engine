#include <bits/stdc++.h>
using namespace std;


#define ll long long
#define INF ((ll)(1e9+7))
#define fo(i, n) for(ll i=0;i<((ll)n);i++)
#define deb(x) cout << #x << " = " << (x) << endl;
#define deb2(x, y) cout << #x << " = " << (x) << ", " << #y << " = " << (y) << endl
#define pb push_back
#define mp make_pair
#define F first
#define S second
#define LSOne(S) ((S) & (-S))
#define all(x) x.begin(), x.end()
#define rall(x) x.rbegin(), x.rend()
typedef pair<int, int> pii;
typedef pair<ll, ll> pl;
typedef vector<int> vi;
typedef vector<ll> vl;
typedef vector<pii> vpii;
typedef vector<pl> vpl;
typedef vector<vi> vvi;
typedef vector<vl> vvl;
typedef vector<vpii> vvpii;
typedef vector<vpl> vvpl;

vvl grid;
vector<tuple<ll, ll, ll, ll>> rooms;

bool check_room(ll x1, ll y1, ll x2, ll y2){
    if(x1 < 0 || y1 < 0 || x2 >= grid.size() || y2 >= grid[0].size()) return false;
    for(ll i = x1; i <= x2; i++){
        for(ll j = y1; j <= y2; j++){
            if(grid[i][j]!=-1) return false;
        }
    }
    return true;
}

void place_room(ll x1, ll y1, ll x2, ll y2, ll val = rooms.size()){
    for(ll i = x1; i <= x2; i++){
        for(ll j = y1; j <= y2; j++){
            grid[i][j] = val;
        }
    }
}

vpl dirs = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

void generate_rooms(ll amount_rooms, ll min_size, ll max_size){
    
    while(rooms.size() < amount_rooms){
        // deb(rooms.size());
        ll x1 = rand() % grid.size();
        ll y1 = rand() % grid[0].size();
        ll x2 = x1 + (rand() % (max_size - min_size + 1)) + min_size;
        ll y2 = y1 + (rand() % (max_size - min_size + 1)) + min_size;
        
        if(!check_room(x1-2, y1-2, x2+2, y2+2)) continue;
        place_room(x1, y1, x2, y2);
        rooms.pb(make_tuple(x1, y1, x2, y2));
    }

    for(ll i = 0; i < rooms.size()*100; i++){
        ll room = rand() % rooms.size();
        ll x1, x2, y1, y2;
        tie(x1, y1, x2, y2) = rooms[room];
        pl dir;
        if(rand()%2) dir = {(x1+x2)/2<grid.size()/2?1:-1, 0};
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

struct UnionFind{
    vl p;
    ll n;
    UnionFind(ll n){
        p.assign(n, -1);
        fo(i, n) p[i] = i;
    }
    ll find(ll x){
        if(p[x] == x) return x;
        return p[x] = find(p[x]);
    }
    
    void unite(ll a, ll b){
        a = find(a);
        b = find(b);
        if(a == b) return;
        p[a] = b;
    }
    
};

void generate_paths(){
    vpl rooms_pos;
    fo(i, rooms.size()){
        ll x1, y1, x2, y2;
        tie(x1, y1, x2, y2) = rooms[i];
        rooms_pos.pb({(rand()%(x2-x1+1))+x1, (rand()%(y2-y1+1))+y1});
    }
    priority_queue<tuple<double, ll, ll>> pq;
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
        tie(x1, y1) = rooms_pos[i];
        tie(x2, y2) = rooms_pos[j];
        while(x1 != x2 || y1 != y2){
            if(rand()%2){
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

int main() {
    cin.tie(0)->sync_with_stdio(0);

    ll n = 100;
    grid.assign(n, vl(n, -1));
    

    generate_rooms(15, 5, 10);
    generate_paths();






    // print
    cout << "Rummen: ";
    fo(i, grid.size()) {
        fo(j, grid[i].size()) {
            if(grid[i][j] == -1) cout << " ";
            else if(grid[i][j] == -2) cout << "#";
            else cout << char(grid[i][j]+'a');
        }
        cout << endl;
    }

    return 0;
}