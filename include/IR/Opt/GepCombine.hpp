#pragma once
#include "../../lib/CFG.hpp"
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"

class AnalysisManager;

class GepCombine : public _PassBase<GepCombine, Function>
{
    class HandleNode
    {
      private:
        BasicBlock *block;
        DominantTree *DomTree;
        DominantTree::TreeNode *dom_node;
        bool Processed = false;
        std::list<DominantTree::TreeNode*>::iterator Curiter;
        std::list<DominantTree::TreeNode*>::iterator Enditer;
        std::unordered_set<GepInst*> Geps;
        std::unordered_set<GepInst*> ChildGeps;
      public:
        std::list<DominantTree::TreeNode*>::iterator Child() { return Curiter; }
        std::list<DominantTree::TreeNode*>::iterator NextChild() { return Curiter++; }
        std::list<DominantTree::TreeNode*>::iterator EndIter() { return Enditer; }
        bool isProcessed()
        {
            return Processed;
        }
        void Process()
        {
            Processed = true;
        }
        BasicBlock *GetBlock()
        {
            return block;
        }
        void SetChildGeps(std::unordered_set<GepInst*> geps)
        {
            ChildGeps = geps;
        }
        std::unordered_set<GepInst*> GetChildGeps()
        {
            return ChildGeps;
        }
        std::unordered_set<GepInst*>& GetGeps()
        {
            return Geps;
        }
        void SetGeps(std::unordered_set<GepInst*> geps)
        {
            Geps = geps;
        }
        HandleNode(DominantTree *dom, DominantTree::TreeNode *node,
                std::list<DominantTree::TreeNode*>::iterator child,
                std::list<DominantTree::TreeNode*>::iterator end,
                std::unordered_set<GepInst*> geps)
            : DomTree(dom), dom_node(node), Curiter(child), Enditer(end), Geps(geps)
        {
            block = node->curBlock;
        }
    };

  private:
    DominantTree *DomTree;
    Function *func;
    AnalysisManager &AM;
    std::vector<Instruction*>wait_del;
    bool ProcessNode(HandleNode *node);
    bool CanHandle(GepInst* src, GepInst* base);
    Value* SimplifyGepInst(GepInst* inst, std::unordered_set<GepInst*>& geps);
    GepInst* HandleGepPhi(GepInst* inst, std::unordered_set<GepInst*>& geps);
    GepInst* Normal_Handle_With_GepBase(GepInst* inst, std::unordered_set<GepInst*>& geps);
  public:
    GepCombine(Function *func, AnalysisManager &AM_) : func(func), AM(AM_)
    {
    }
    bool run();
};