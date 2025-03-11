// #include "../../lib/cfg/cfgg.hpp"
// #include "CoreClass.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include <numeric>
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
        // dfs的时候初始化
        bool visited;
        int dfs_order;
        TreeNode* dfs_fa;

        // init 的时候初始化
        std::list<TreeNode*> predNodes;  // 前驱
        std::list<TreeNode*> succNodes; //  后继

        // 构造函数初始化
        TreeNode* sdom;
        TreeNode* idom;
        // BasicBlock* curBlock; 建了BasicBlock* 与 TreeNode* 映射的表 

        TreeNode()
            :visited(false),dfs_order(0),sdom(this),// 初始化的时候初始化成this自己本身sdom
            idom(nullptr)
        {}
    };

    struct dsuNode
    {
        TreeNode* father;
        TreeNode* min_sdom;
        int record;   // 记录的这个不是dfs的排序，而是bbs数组 1，2，3，4这样，可能之后有用
        dsuNode()
            :father(nullptr),min_sdom(nullptr),
            record(0)
        {}
    };

    Function* _func;    

    std::vector<BasicBlock*> BasicBlocks;
    std::vector<TreeNode*> Nodes;

    // 建立了Basicblock与TreeNode的映射关系
    std::map<BasicBlock*,TreeNode*> BlocktoNode;

    // DSU并查集一般用数组实现
    std::vector<dsuNode*> DSU;

    size_t BBsNum;
    int count = 1;  // dfs的时候计数赋值的
public:
    DominantTree(Function* func)
        :_func(func), Nodes(func->Size()),count(1),
         BBsNum(func->Size()),DSU(func->Size()+1)
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

                BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des_true]);
                BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des_false]);

                BlocktoNode[bb]->predNodes.push_back(BlocktoNode[des_true]);
                BlocktoNode[bb]->predNodes.push_back(BlocktoNode[des_false]);
            }
            else if(UnCondInst* uncondInst = dynamic_cast<UnCondInst*>(Inst))
            {
                auto &uselist = uncondInst->GetUserUseList();
                BasicBlock* des = dynamic_cast<BasicBlock*>(uselist[0]->GetValue());

                BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des]);
                BlocktoNode[bb]->predNodes.push_front(BlocktoNode[des]);
            }
        }
    }

    void BuildDominantTree() 
    {
        // 因为我实现了对应的关系
        // DFS(BlocktoNode[BlocktoNode[0]]);
        // DFS(Nodes[0])
        InitNodes();
        BasicBlock *Bbegin = *(_func->begin());
        DFS(BlocktoNode[Bbegin]);
        InitDSU();
    }

    void GetIdom()
    {
        for(int i = BBsNum; i > 1; i--)
        {
            TreeNode* father,sdom_cadidate;
            TreeNode* node = BlocktoNode[BasicBlocks[BBsNum]];
            int dfs_order = node->dfs_order;
            father = Nodes[dfs_order]->dfs_fa;
            for(auto e : Nodes[dfs_order]->predNodes){
                // ??? 是不是逻辑错了
                if(e->dfs_order != 0){
                    sdom_cadidate = 
                    std::min(sdom_cadidate,eval(e));
                }
            }
        }
    }

    TreeNode* find(TreeNode* node)
    {
        //并查集实现查找的策略
        if(node == DSU[node->dfs_order]->father)
            return node;
        TreeNode* tmp = DSU[node->dfs_order]->father;
        // DSU[node->dfs_order] = find(); 
    }

    TreeNode* eval(TreeNode* node){

    }

    void InitDSU()
    {
        for(int i = 1; i <= BBsNum; i++)
        {
            DSU[i]->record = i;
            DSU[i]->father = Nodes[i-1];
            DSU[i]->min_sdom = Nodes[i-1];
        }
    }

    void DFS(TreeNode* pos)
    {
        pos->visited = true;
        pos->dfs_order = count;
        count++;
        for(auto e : pos->predNodes){
            if(!e->visited){
                DFS(e);
                e->dfs_fa = pos;
            }
        }
    }

    bool dominates(BasicBlock* bb1,BasicBlock* bb2);
    ~DominantTree() = default;
};







