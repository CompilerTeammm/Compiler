#include "IDF.hpp"
#include<queue>
#include <utility>
#include<vector>

struct less_second {
  template <typename T> bool operator()(const T &lhs, const T &rhs) const {
    return lhs.second < rhs.second;
  }
};

class BasicBlock;
void IDFCalculator::calculate(std::vector<BasicBlock*>& PHIBlocks)
{
    // 计算支配树的层级
    if(DomLevels.empty()){
          DomLevels = _DT.DomLevels;
          // caculateLevel(_DT.Nodes[0],0);
    }

    typedef std::pair<TreeNode*,int> DomTreeNodePair;
    typedef std::priority_queue<DomTreeNodePair,
                            std::vector<DomTreeNodePair>,
                            less_second> IDFpriorityQueue; //优先队列默认是大堆，构造小堆
    //优先级队列
    IDFpriorityQueue PQ;


    for(BasicBlock* BB:*DefBlocks){
        //建立了Node和level的优先级队列
        if(TreeNode* Node = _DT.getNode(BB))
            PQ.push(std::make_pair(Node,DomLevels[Node]));
    }

    std::vector<TreeNode*>  WorkList;
    std::set<TreeNode*> VisitedPQ;
    std::set<TreeNode*> VisitedWorklist;

    // 前期准备工作完成，算法核心
    while(!PQ.empty()){
        // PQ取出优先级最高的
        DomTreeNodePair RootPair = PQ.top();
        PQ.pop();
        TreeNode* Root = RootPair.first;
        int RootLevel = RootPair.second;

        WorkList.clear();
        WorkList.push_back(Root);
        VisitedWorklist.insert(Root);

        //...
        while(!WorkList.empty()){
            // llvm自己实现的一套数据结构，即可以back又可以 pop_back
            TreeNode* node = WorkList.back();
            WorkList.pop_back();
            BasicBlock* BB = node->curBlock;

            //遍历在CFG中后继结点
            for(TreeNode* succNode : node->succNodes)
            {
                //该边是支配树内部边
                if(succNode->idom == node)
                  continue;
                
                //避免向上跨越层级
                int SuccLevel = DomLevels[succNode];
                if(SuccLevel > RootLevel)
                  continue;
                
                if(!VisitedPQ.insert(succNode).second)
                  continue;
                
                BasicBlock *succBB = succNode->curBlock;
                if(useLiveIn && !LiveInBlocks->count(succBB))
                  continue;
                
                PHIBlocks.emplace_back(succBB);
                if(!DefBlocks->count(succBB))
                  PQ.push(std::make_pair(succNode, SuccLevel));
            }

            for(auto DomChild : node->idomChild)
            {
              if(VisitedWorklist.insert(DomChild).second)
                  WorkList.push_back(DomChild);
            }
        }
    }
}