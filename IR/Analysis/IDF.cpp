#include "IDF.hpp"
#include<queue>
#include<vector>

struct less_second {
  template <typename T> bool operator()(const T &lhs, const T &rhs) const {
    return lhs.second < rhs.second;
  }
};

class BasicBlock;
void IDFCalculator::calculate(std::vector<BasicBlock*>& IDFBlocks)
{
    if(DomLevels.empty()){

    }

    typedef std::pair<TreeNode*,int> DomTreeNodePair;
    typedef std::priority_queue<DomTreeNodePair,
                            std::vector<DomTreeNodePair>,
                            less_second> IDFpriorityQueue;
    IDFpriorityQueue PQ;

    for(BasicBlock* BB:*DefBlocks){
        if(TreeNode* Node = _DT.getNode(BB))
            PQ.push(std::make_pair(Node,DomLevels[Node]));
    }

    std::vector<TreeNode*>  WorkList;
    std::set<TreeNode*> VisitedPQ;
    std::set<TreeNode*> VisitedWorklist;

    while(!PQ.empty()){
        DomTreeNodePair RootPair = PQ.top();
        PQ.pop();
        TreeNode* Root = RootPair.first;
        int RootLevel = RootPair.second;

        WorkList.clear();
        WorkList.push_back(Root);
        VisitedWorklist.insert(Root);

        while(!WorkList.empty()){
            // llvm自己实现的一套数据结构，即可以back又可以 pop_back
            // 给我人看傻了
            TreeNode* node = WorkList.back();
            WorkList.pop_back();
            BasicBlock* bb = node->thisB
        }
    }
}