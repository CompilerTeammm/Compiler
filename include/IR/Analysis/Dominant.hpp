#include "../../lib/cfg/cfgg.hpp"
#include <vector>
// 搭建支配树
class DominantTree
{
public:
    // 实现遍历的 dfs
    void DFS(std::vector<BasicBlock*> cfg,std::vector<std::vector<int>> grap)
    {
        int visited = 0;

    }
    // sdom 初始化为自己
    DominantTree()
    :sdom() {};

    class Node
    {

    };

private:
    std::vector<BasicBlock*> pred; // 
    std::vector<BasicBlock*> parent;
    std::vector<BasicBlock*> idom; // 直接支配节点数组 
    std::vector<BasicBlock*> sdom;  // 半支配节点数组
};


