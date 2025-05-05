#ifndef UNION_FIND_H
#define UNION_FIND_H

#include <vector>
#include <utility>
#include <iostream>

struct UnionFind {
    std::vector<long long> p;
    
    explicit UnionFind(long long n) {
        p.assign(n, -1);
        for (long long i = 0; i < n; i++) 
            p[i] = i;
    }
    
    long long find(long long x) {
        if (p[x] == x) 
            return x;
            
        return p[x] = find(p[x]);
    }
    
    void unite(long long a, long long b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        p[a] = b;
    }
};

#endif
