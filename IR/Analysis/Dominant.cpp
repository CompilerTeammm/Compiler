#pragma once
#include "../../include/IR/Analysis/Dominant.hpp"
#include <algorithm>
#include <functional>

void DominantTree::InitNodes()
{
    //   pair <BasicBlock* , TreeNode*>
    for (int i = 0; i <= BasicBlocks.size(); i++)
        BlocktoNode[BasicBlocks[i]] = Nodes[i],  // map
            Nodes[i]->curBlock = BasicBlocks[i]; // key-value value不好寻找key

    // 建立了前驱与后继的确定
    for (auto bb : BasicBlocks)
    {
        Instruction *Inst = bb->GetLastInsts();
        if (CondInst *condInst = dynamic_cast<CondInst *>(Inst))
        {
            auto &uselist = condInst->GetUserUseList();
            BasicBlock *des_true = dynamic_cast<BasicBlock *>(uselist[1]->GetValue());
            BasicBlock *des_false = dynamic_cast<BasicBlock *>(uselist[2]->GetValue());

            BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des_true]);
            BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des_false]);

            BlocktoNode[bb]->predNodes.push_back(BlocktoNode[des_true]);
            BlocktoNode[bb]->predNodes.push_back(BlocktoNode[des_false]);
        }
        else if (UnCondInst *uncondInst = dynamic_cast<UnCondInst *>(Inst))
        {
            auto &uselist = uncondInst->GetUserUseList();
            BasicBlock *des = dynamic_cast<BasicBlock *>(uselist[0]->GetValue());

            BlocktoNode[bb]->succNodes.push_front(BlocktoNode[des]);
            BlocktoNode[bb]->predNodes.push_front(BlocktoNode[des]);
        }
    }
}

void DominantTree::BuildDominantTree()
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
    caculateLevel(Nodes[0], 0);
}

void DominantTree::buildTree()
{
    for (int i = 1; i < BBsNum; i++)
    {
        TreeNode *idom_node = Nodes[i]->idom;
        if (idom_node)
            idom_node->idomChild.push_back(Nodes[i]);
    }
}

void DominantTree::InitIdom()
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
        // 遍历前驱
        for (auto e : TN->predNodes)
        {
            // eval(v) 返回 v 到根路径上 最小的 min_sdom
            min = std::min(min, eval(e->dfs_order));
        }
        DSU[i]->min_sdom = min;
        TN->sdom = DSU[min]->Nodesbydfs;
        bucket[min].push_back(i);
        // link(parent,v); 将 v连接到父节点
        link(father_order, i);
        for (auto e : bucket[min])
        {
            int u = eval(e);
            if (DSU[u]->min_sdom == DSU[e]->min_sdom)
                TN->idom = TN->sdom;
            else if (DSU[u]->min_sdom < DSU[e]->min_sdom)
                TN->idom = DSU[u]->Nodesbydfs->idom;
            else
                assert("dominant error");
        }
        bucket[father_order].clear();
    }

    // 根结点不需要跑出来idom
    for (int i = 2; i <= Nodes.size(); i++)
    {
        TreeNode *TN = DSU[i]->Nodesbydfs;
        if (TN->idom != TN->sdom)
            TN->idom = DSU[TN->idom->dfs_order]->Nodesbydfs->idom;
        // TN->idom = DSU[TN->idom->dfs_order]->Nodesbydfs->idom;
    }
}

void DominantTree::InitDSU()
{
    for (int i = 1; i < DSU.size(); i++)
    {
        int order = Nodes[i - 1]->dfs_order;
        DSU[order]->Nodesbydfs = Nodes[i - 1];
        DSU[order]->parent = order;
        DSU[order]->min_sdom = order;
    }
}

void DominantTree::DFS(TreeNode *pos)
{
    pos->visited = true;
    pos->dfs_order = count;
    count++;
    for (auto e : pos->predNodes)
    {
        if (!e->visited)
        {
            DFS(e);
            e->dfs_fa = pos;
        }
    }
}

void DominantTree::caculateLevel(TreeNode *node, int level)
{
    DomLevels[node] = level;
    for (auto child : node->idomChild)
    {
        // 核心就是一个回溯，easy
        if (DomLevels.count(child))
            caculateLevel(child, level + 1);
    }
}

bool DominantTree::dominates(BasicBlock *bb1, BasicBlock *bb2)
{
    TreeNode *node1 = BlocktoNode[bb1];
    TreeNode *node2 = BlocktoNode[bb2];
    TreeNode *tmpNode = nullptr;

    int tmp1 = DomLevels[node1];
    int tmp2 = DomLevels[node2];
    if (tmp1 < tmp2)
    {
        while (tmp2 != tmp1)
        {
            tmpNode = node2->idom;
            tmp2--;
        }

        if (tmpNode == node1)
        {
            return true;
        }
    }

    return false;
}
