// #include "../../lib/cfg/cfgg.hpp"
#include <mutex>
#include <vector>
#include<iostream>
// 搭建支配树
// 步骤： 1. DFS , 得到DFS树和标号
// 2. 逆序遍历 求 sdom
// 3. 通过sdom 求得 idom
// 4. 正序遍历一遍得到正确的 idom
// 详情见论文

class Lengauer_Tarjan
{
// 做测试，之后进行修改
//    假设：
// 没有建立图的关系，是在遍历节点之后建立图的关系
public:
    struct Node 
    {
        // 用链表好，还是用数组呢？
        std::vector<Node*> pre;
        std::vector<Node*> succ;

        int flag = 0;
        static int order_num;
    
        bool isVisited()
        {
            return flag != 0;
        }
    };

    Lengauer_Tarjan(int _n,int _m)
        :node_num(_n),
        edge_num(_m)
    { 
        CFG =std::vector<Node> (node_num+1);
    }

    void add_edge(int u, int v);
    void dump();
    void Run();
    void DFS_func();

    int node_num;
    int edge_num;
    int dfs_cnt = 0;
    std::vector<Node> CFG;
    std::vector<Node*> DFS_order;

    std::vector<Node*> sdom;
    std::vector<Node*> idom;
};


