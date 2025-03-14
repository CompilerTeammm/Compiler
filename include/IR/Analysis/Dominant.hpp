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
        TreeNode* Nodesbydfs;
        int record;   // 记录的这个不是dfs的排序，而是bbs数组 1，2，3，4这样，可能之后有用
        dsuNode()
            :father(nullptr),min_sdom(nullptr),
            record(0),Nodesbydfs(0)
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
        // DSU的初始化再DFS之后，我需要依据DFS的序号来建立关系
        InitDSU();
        GetIdom();
    }

    void GetIdom()
    {
        // 逆序遍历，从dfs最大的结点开始, sdom求取
        // 需要记录最小的sdom对吧？
        for (int i = Nodes.size(); i >= 1; i--)
        {
            TreeNode *min, tmp;
            min->dfs_order = 1000000;
            TreeNode *TN = DSU[i]->Nodesbydfs;
            for (auto e : TN->predNodes)
            {
                min->dfs_order = std::min(min->dfs_order,eval(e));
                // if ((e->dfs_order < i))
                // {
                //     if((e->dfs_order < min->dfs_order))
                //         min = e;
                // }
                // else //e->dfs_order > i 
                // {
                //     min = find(e->dfs_order);
                // }
            }
            DSU[i]->min_sdom = min;
        }
    }

    // 我把DSU和Nodes也建立了联系
    // Nodes.dfs_num = DSU 的序号 1，2，3，4
    // DSU的TreeNodes* 指针可以找到对应的 Nodes   TreeNode* TN = DSU[i]->Nodesbydfs;
    void InitDSU()
    {
        for(int i = 1; i < DSU.size();i ++)
        {
            int order = Nodes[i-1]->dfs_order;
            DSU[order]->Nodesbydfs = Nodes[i-1];
            DSU[order]->father = Nodes[i-1]->dfs_fa;
        }
    }

    TreeNode* find(TreeNode* node)
    {
        
    }

    int eval(TreeNode* node)
    {

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







