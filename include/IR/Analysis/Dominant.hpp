#include "../../lib/cfg/cfgg.hpp"
#include <vector>
// 搭建支配树
class Lengauer_Tarjan
{
public:
    struct Node
    {
        std::vector<Node*> pred;
        std::vector<Node*> succ;

        int dfs_num = 0;

    };
public:
    // 实现遍历的 dfs
    void DFS(std::vector<Node*> cfg,std::vector<std::vector<int>> grap)
    {
        int visited = 0;
        // if(int i = 1; i <size+1; i++)
            DFS();
    };
    // sdom 初始化为自己
    Lengauer_Tarjan(int st)
    :sdom() {};

    void DUS()
    {

    };

    std::vector<Node*>& add_edge(int u, int v)
    {
        
    }

private:
    int st;
    std::vector<Node*> cfg;
    std::vector<Node*> sdom;
    std::vector<Node*> idom;
};


