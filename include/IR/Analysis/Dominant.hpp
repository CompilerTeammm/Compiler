// #include "../../lib/cfg/cfgg.hpp"
// #include "CoreClass.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include <vector>
#include<iostream>
// 搭建支配树
// 步骤： 1. DFS , 得到DFS树和标号
// 2. 逆序遍历 求 sdom
// 3. 通过sdom 求得 idom
// 4. 正序遍历一遍得到正确的 idom
// 详情见论文

using BBPtr = std::unique_ptr<BasicBlock>;
class DominantTree
{
private:
    Function* _func;
    std::vector<std::unique_ptr<BasicBlock>> BasicBlocks;

    // 记录遍历的DFSnums的顺序
    std::vector<int> DFSnums;
public:
    // 输入的应该是func，func->BBs 
    DominantTree(Function* func)
        :_func(func),DFSnums(func->Size())
    {   
        BasicBlocks = _func->GetBBs();
    } 

    void BuildDominantTree() 
    {
        UnCondInst
    }

    static std::vector<int>& DFS(int pos)
    {
        
    }

    bool dominates(BasicBlock* bb1,BasicBlock* bb2);
};












// // #include "../../lib/cfg/cfgg.hpp"
// #include <mutex>
// #include <vector>
// #include<iostream>
// // 搭建支配树
// // 步骤： 1. DFS , 得到DFS树和标号
// // 2. 逆序遍历 求 sdom
// // 3. 通过sdom 求得 idom
// // 4. 正序遍历一遍得到正确的 idom
// // 详情见论文

// class Lengauer_Tarjan
// {
// // 做测试，之后进行修改
// //    假设：
// // 没有建立图的关系，是在遍历节点之后建立图的关系
// public:
//     struct Node 
//     {
//         // 用链表好，还是用数组呢？
//         std::vector<Node*> pre;
//         std::vector<Node*> succ;

//         // sdom  idom 是在node中声明
//         Node* sdom;
//         Node* idom;

//         int flag = 0;
//         int order_num = 0;
//         bool isVisited()
//         {
//             return flag != 0;
//         }
//     };

//     Lengauer_Tarjan(int _n,int _m)
//         :node_num(_n),
//         edge_num(_m)
//     { 
//         CFG =std::vector<Node> (node_num+1);
//     }

//     void add_edge(int u, int v);
//     void dump();
//     void Run();
//     void DFS_func();
//     void get_idom();

//     int node_num;
//     int edge_num;
//     // static int order_num;
//     // int dfs_cnt = 0;
//     std::vector<Node> CFG;

//     // DFS_order[1] = 1   1号节点，第 1 个在dfs树上被访问
//     // DFS_order[5] = 2   5号节点，第 2 个在dfs树上被访问
//     // 我使用 vector<int> 实际上达到了一种映射关系 
//     std::vector<int> DFS_order;
//     // std::vector<Node*> sdom;
//     // std::vector<Node*> idom;
// };


