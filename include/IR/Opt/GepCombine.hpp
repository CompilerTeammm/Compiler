#pragma once
#include "../../lib/CFG.hpp"
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../include/IR/Analysis/SideEffect.hpp"
#include "../../include/IR/Opt/ConstantFold.hpp"

class AnalysisManager;

class GepCombine : public _PassBase<GepCombine, Function>
{
    // 内部节点处理器
    class NodeContext
    {
        BasicBlock* blk;
        DominantTree* domTree;
        DominantTree::TreeNode* treeNode;
        bool processed = false;
        std::list<DominantTree::TreeNode*>::iterator childIt;
        std::list<DominantTree::TreeNode*>::iterator childEnd;
        std::unordered_set<GepInst*> localGeps;
        std::unordered_set<GepInst*> subGeps;

    public:
        NodeContext(DominantTree* dt, DominantTree::TreeNode* tn,
                    std::list<DominantTree::TreeNode*>::iterator begin,
                    std::list<DominantTree::TreeNode*>::iterator end,
                    std::unordered_set<GepInst*> geps)
            : domTree(dt), treeNode(tn), childIt(begin), childEnd(end), localGeps(geps)
        {
            blk = tn->curBlock;
        }

        BasicBlock* Block() const { return blk; }
        bool IsProcessed() const { return processed; }
        void MarkProcessed() { processed = true; }

        auto ChildIterator() { return childIt; }
        auto NextChild() { return childIt++; }
        auto EndIterator() { return childEnd; }
        std::unordered_set<GepInst*>& Geps() { return localGeps; }
        const std::unordered_set<GepInst*>& Geps() const { return localGeps; }
        void SetGeps(const std::unordered_set<GepInst*>& geps) { localGeps = geps; }

        const std::unordered_set<GepInst*>& ChildGeps() const { return subGeps; }
        void SetChildGeps(const std::unordered_set<GepInst*>& geps) { subGeps = geps; }
    };

private:
    DominantTree* domTree;
    Function* func;
    AnalysisManager& analysisMgr;
    std::vector<Instruction*> pendingDelete;

    bool ProcessNode(NodeContext* nodeCtx);
    bool CanCombineGep(GepInst* src, GepInst* base);
    Value* SimplifyGep(GepInst* inst, std::unordered_set<GepInst*>& geps);
    GepInst* ProcessGepPhi(GepInst* inst, std::unordered_set<GepInst*>& geps);
    GepInst* ProcessNormalGep(GepInst* inst, std::unordered_set<GepInst*>& geps);

public:
    GepCombine(Function* f, AnalysisManager& am) : func(f), analysisMgr(am) {}
    bool run();
};