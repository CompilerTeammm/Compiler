#include "../include/IR/Analysis/Dominant.hpp"
#include <iostream>

int main()
{
    int n, m;
    std::cin >> n >> m;
    Lengauer_Tarjan lt(n,m);
    for(int i = 1; i <= lt.edge_num; i++)
    {
        int u, v;
        std::cin >> u >> v;
        lt.add_edge(u,v);
    }


    lt.Run();
    lt.dump();
    return 0;
}