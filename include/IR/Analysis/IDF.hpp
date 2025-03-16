#pragma once
#include "Dominant.hpp"
#include <set>

// iterated D F  迭代支配边界
class IDFCalculator
{
    using TreeNode = DominantTree::TreeNode;
public:
    IDFCalculator(DominantTree &DT):
            _DT(DT),useLiveIn(false)  {}

    void setLiveInBlocks(std::set<BasicBlock*>& blocks)
    {
        LiveInBlocks = &blocks;
        useLiveIn = true;
    }

    void resetLiveInBlocks(std::set<BasicBlock*>& blocks)
    {
        LiveInBlocks = nullptr;
        useLiveIn = false;
    }

    void setDefiningBlocks(std::set<BasicBlock *>& blocks){
        DefBlocks = &blocks;
    }

    // level = 0; 为最高级的level
    void caculateLevel(TreeNode* node,int level)
    {
        DomLevels[node] = level;
        for(auto child : node->idomChild)
        {
            if(DomLevels.count(child))
                caculateLevel(child, level+1);
        }
    }

    void calculate(std::vector<BasicBlock*>& IDFBlocks);

private:
    DominantTree& _DT;
    bool useLiveIn;

    std::set<BasicBlock*> * LiveInBlocks;
    std::set<BasicBlock*>* DefBlocks;

    //这个用来记录tree leves 层级
    std::map<TreeNode*,int> DomLevels;

    std::vector<BasicBlock*> PHIBlocks;
};