// #include "../../lib/cfg/cfgg.hpp"
// #include "CoreClass.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include <utility>
#include <vector>
#include<iostream>
// 搭建支配树
// 步骤： 1. DFS , 得到DFS树和标号
// 2. 逆序遍历 求 sdom
// 3. 通过sdom 求得 idom
// 4. 正序遍历一遍得到正确的 idom
// 详情见论文

// 不在BasicBlock里面搞，在DominantTree确定前驱与后继
// 一旦优化初始化了，DominantTree就已经搭建好了，可以有前驱后继，支配等条件
struct TreeNode;
using BBPtr = std::unique_ptr<BasicBlock>;
class DominantTree
{
private:
    // 输入的应该是func，func->BBs 
    // Node 要和 BasicBlock一一对应
    struct TreeNode  // 实际上称为了BBs
    {
        bool visited;
        int dfs_order;
        std::list<TreeNode*> predNodes;  // 前驱
        std::list<TreeNode*> succNodes; //  后继

        TreeNode* dfsfa;
        TreeNode* sdom;
        TreeNode* idom;
        // BasicBlock* curBlock; 建了BasicBlock* 与 TreeNode* 映射的表 

        TreeNode()
            :visited(false),dfs_order(0),sdom(this),
            idom(nullptr)
        {}
    };

    Function* _func;    

    std::vector<BasicBlock*> BasicBlocks;
    std::vector<TreeNode*> Nodes;
    std::map<BasicBlock*,TreeNode*> BlocktoNode;
    int count = 1;
public:
    DominantTree(Function* func)
        :_func(func), Nodes(func->Size()),count(1)
    {   
        for(auto& e : _func->GetBBs())
        {
            // 用release太危险了，之后我也会用func，去调用BBs，如果销毁了还去调用太危险了
            BasicBlocks.push_back(e.get());
        }
    } 

    void InitNodes()
    {
        //   pair <BasicBlock* , TreeNode*>
        for(int i = 0; i <= BasicBlocks.size() ; i++) 
            BlocktoNode[BasicBlocks[i]] = Nodes[i]; 
        // 建立了前驱与后继的确定
        for(auto bb : BasicBlocks) 
        {
            Instruction* Inst = bb->GetLastInsts();
            if(CondInst *condInst = dynamic_cast<CondInst*> (Inst))
            {
                auto &uselist = condInst->GetUserUseList();
                BasicBlock *des_true = dynamic_cast<BasicBlock *>(uselist[1]->GetValue());
                BasicBlock *des_false = dynamic_cast<BasicBlock *>(uselist[2]->GetValue());

                Nodes[bb->index]->succNodes.push_front(BlocktoNode[des_true]);
                Nodes[bb->index]->succNodes.push_front(BlocktoNode[des_false]);

                Nodes[bb->index]->predNodes.push_back(BlocktoNode[des_true]);
                Nodes[bb->index]->predNodes.push_back(BlocktoNode[des_false]);
            }
            else if(UnCondInst* uncondInst = dynamic_cast<UnCondInst*>(Inst))
            {
                auto &uselist = uncondInst->GetUserUseList();
                BasicBlock* des = dynamic_cast<BasicBlock*>(uselist[0]->GetValue());

                Nodes[bb->index]->succNodes.push_front(BlocktoNode[des]);
                Nodes[bb->index]->predNodes.push_front(BlocktoNode[des]);
            }
        }
    }

    void BuildDominantTree() 
    {

    }

    std::vector<int> &DFS(int pos)
    {
        Nodes[pos]->visited = true;
        Nodes[pos]->dfs_order = count;
        count++;
        if (Nodes[pos]->visited && )
            DFS();
    }

    bool dominates(BasicBlock* bb1,BasicBlock* bb2);
};







