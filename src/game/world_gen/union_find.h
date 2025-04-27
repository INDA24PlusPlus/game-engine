#ifndef UNION_FIND_H
#define UNION_FIND_H

#include "definitions.h"

struct UnionFind {
    vl p;
    
    explicit UnionFind(ll n) {
        p.assign(n, -1);
        fo(i, n) p[i] = i;
    }
    
    ll find(ll x) {
        if(p[x] == x) return x;
        return p[x] = find(p[x]);
    }
    
    void unite(ll a, ll b) {
        a = find(a);
        b = find(b);
        if(a == b) return;
        p[a] = b;
    }
};

#endif 