#include"../../include/IR/Analysis/Dominant.hpp"
#include <functional>

void Lengauer_Tarjan::add_edge(int u,int v)
{
    CFG[u].succ.push_back(&CFG[v]);
    CFG[v].pre.push_back(&CFG[u]);
}

void Lengauer_Tarjan::DFS_func()
{
    DFS_order = std::vector<Node*> (node_num + 1);

    Lengauer_Tarjan::Node::order_num = 0;
    std::function<void(Node*)> DFS;
    DFS = [&](Node* pos ) -> void
    {
        for(auto e : pos->succ)
        {
            if(!e->isVisited())
            {
                Lengauer_Tarjan::Node::order_num++;
                e->flag = Lengauer_Tarjan::Node::order_num;
                dfs_cnt++;
                DFS(&(*e));
            }
        }
    };
    DFS(&CFG[1]);
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