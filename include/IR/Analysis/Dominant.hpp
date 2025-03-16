// #include "../../lib/cfg/cfgg.hpp"
// #include "CoreClass.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "IDF.hpp"
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

// 辅助森林和并查集的关系  
// 并查集 通过 link 和 eval 操作，维护一个支持路径压缩的森林结构

// debug 等有了测试样例调试的时候看看

# define MAX_ORDER 1000000
# define N 10000
struct TreeNode;
using BBPtr = std::shared_ptr<BasicBlock>;
class DominantTree
{
    friend class IDFCalculator;
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
        BasicBlock* curBlock;
        std::list<TreeNode*> idomChild;

        // 构造函数初始化
        TreeNode* sdom;
        TreeNode* idom;
        // BasicBlock* curBlock; 建了BasicBlock* 与 TreeNode* 映射的表 

        TreeNode()
            :visited(false),dfs_order(0),sdom(this),// 初始化的时候初始化成this自己本身sdom
            idom(nullptr)
        {}
    };

    //重新设计，不采用指针的形式，化简成int形式
    struct dsuNode
    {
        int parent;            //都变成了dfs_order 
        int min_sdom;          //都变成了dfs_order
        TreeNode *Nodesbydfs;  // 寻找Nodes中Node的关键,仅此一个指针与Nodes建立关系
   
        dsuNode()
            : parent(0), min_sdom(0),
              Nodesbydfs(nullptr) // record(0),
        { }
    };

    // struct dsuNode
    // {
    //     TreeNode* parent;
    //     TreeNode* min_sdom;
    //     TreeNode* Nodesbydfs;  // 寻找Nodes中Node的关键
    //     //int record;   // 记录的这个不是dfs的排序，而是bbs数组 1，2，3，4这样，可能之后有用
    //     dsuNode()
    //         :parent (nullptr),min_sdom(nullptr),
    //         Nodesbydfs(0)//record(0),
    //     {}
    // };

    Function* _func;    

    std::vector<BasicBlock*> BasicBlocks;
    std::vector<TreeNode*> Nodes;

    // 建立了Basicblock与TreeNode的映射关系
    std::map<BasicBlock*,TreeNode*> BlocktoNode;

    // DSU并查集一般用数组实现
    std::vector<dsuNode*> DSU;

    // 维护一个 bucket  sdom 为 u 的点集
    // 这个就是一个二维数组
    std::vector<int> bucket[N];

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

    TreeNode* getNode(BasicBlock* BB)
    {
        return BlocktoNode[BB];
    }

    void InitNodes()
    {
        //   pair <BasicBlock* , TreeNode*>
        for(int i = 0; i <= BasicBlocks.size() ; i++) 
            BlocktoNode[BasicBlocks[i]] = Nodes[i],  // map
            Nodes[i]->curBlock = BasicBlocks[i];     // key-value value不好寻找key
            
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
        InitIdom();
        // 到这里为止 Nodes 里面的idom和sdom全部被记录了下来了
        // Nodes[0] 是没有idom的信息的为nullptr
    }

    // 这个遍历也只是可以找到他们的孩子了
    void buildTree()
    {
        for(int i = 1; i < BBsNum; i++)
        {
            TreeNode* idom_node = Nodes[i]->idom;
            if(idom_node)
                idom_node->idomChild.push_back(Nodes[i]);
        }
    }

    void InitIdom()
    {
        // 逆序遍历，从dfs最大的结点开始, sdom求取
        // 需要记录最小的sdom对吧
        for (int i = Nodes.size(); i >= 1; i--)
        {
            // int min;
            // TreeNode* tmp = nullptr;
            int min = MAX_ORDER;
            TreeNode *TN = DSU[i]->Nodesbydfs;
            int father_order = TN->dfs_fa->dfs_order;
            //遍历前驱
            for (auto e : TN->predNodes)
            {
                // eval(v) 返回 v 到根路径上 最小的 min_sdom
                min = std::min(min,eval(e->dfs_order));
            }
            DSU[i]->min_sdom = min;
            TN->sdom = DSU[min]->Nodesbydfs;
            bucket[min].push_back(i);
            // link(parent,v); 将 v连接到父节点 
            link(father_order,i);
            for(auto e :bucket[min])
            {
                int u = eval(e);
                if(DSU[u]->min_sdom == DSU[e]->min_sdom)
                    TN->idom = TN->sdom;
                else if(DSU[u]->min_sdom < DSU[e]->min_sdom)
                    TN->idom = DSU[u]->Nodesbydfs->idom;
                else
                    assert("dominant error");
            }
            bucket[father_order].clear();
        }

        // 根结点不需要跑出来idom
        for(int i = 2; i <= Nodes.size(); i++)
        {
            TreeNode *TN = DSU[i]->Nodesbydfs;
            if(TN->idom != TN->sdom)
                TN->idom = DSU[TN->idom->dfs_order]->Nodesbydfs->idom;
                // TN->idom = DSU[TN->idom->dfs_order]->Nodesbydfs->idom;
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
            DSU[order]->parent = order;
            DSU[order]->min_sdom = order;
        }
    }

    // 跟新 DSU[v]结点中的 parent结点
    void link(int parent, int v)
    {
        DSU[v]->parent = parent;
    }

    // 传入的是结点 TreeNode* 
    int eval(int order)
    {
        if(DSU[order]->parent == order)
            return DSU[order]->min_sdom;

        eval(DSU[order]->parent);
        if(DSU[DSU[order]->parent]->min_sdom < DSU[order]->min_sdom)
             DSU[order]->min_sdom = DSU[DSU[order]->parent]->min_sdom;
        DSU[order]->parent = DSU[DSU[order]->parent]->parent;  // 压缩路径
        
        return DSU[order]->min_sdom;
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

    bool dominates(BasicBlock* bb1,BasicBlock* bb2)
    {
        TreeNode* node1 = BlocktoNode[bb1];
        TreeNode* node2 = BlocktoNode[bb2];


    }

    ~DominantTree() = default;
};







