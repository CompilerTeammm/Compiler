#include"../../include/IR/Analysis/Dominant.hpp"
#include <algorithm>
#include <functional>

int order = 1;
void Lengauer_Tarjan::add_edge(int u,int v)
{
    CFG[u].succ.push_back(&CFG[v]);
    CFG[v].pre.push_back(&CFG[u]);
}

void Lengauer_Tarjan::DFS_func()
{
    // DFS_order = std::vector<int> (node_num + 1);

    std::function<void(Node*)> DFS;
    DFS = [&](Node* pos ) -> void
    {
        pos->flag = 1;
        pos->order_num = order;
        pos->sdom = pos;
        for(auto e : pos->succ)
        {
            if(!e->isVisited())
            {
                order++;
                DFS(e);
            }
        }
    };
    DFS(&CFG[1]);

    for(auto& e :CFG)
    {
        DFS_order.push_back(e.order_num);
    }
}


void Lengauer_Tarjan::Run()
{
    Lengauer_Tarjan::DFS_func();

}

void Lengauer_Tarjan::dump()
{
    std::cout<<node_num<<" "<<edge_num<<std::endl;

}

    // for(int i = 1; i <= edge_num; i++)
    // {
    //     for(auto e:CFG[i].pre)
    //     {
    //         std::cout<<std::endl;
    //     } 
    // }