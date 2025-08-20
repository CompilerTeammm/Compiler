#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"

class AnalysisManager;
using ValueMap = std::unordered_map<Value*, std::unordered_map<size_t, Value*>>;

struct AllocaHasher
{
    size_t operator()(AllocaInst* alloc, std::vector<int> idx)
    {
        std::reverse(idx.begin(), idx.end());
        size_t hash = std::hash<AllocaInst*>()(alloc);
        int j = 0;
        for (int v : idx)
        {
            hash += (((std::hash<int>()(v + 3) * 10107 ^ std::hash<int>()(v + 5) * 137) * 157) * ((j + 4) * 107));
            ++j;
        }
        return hash;
    }
};

struct ValueHasher
{
    size_t operator()(Value* val) const
    {
        if (auto cint = dynamic_cast<ConstIRInt*>(val))
        {
            int v = cint->GetVal();
            return ((std::hash<int>()(v + 3) * 10107 ^ std::hash<int>()(v + 5) * 137) * 157);
        }
        return std::hash<Value*>{}(val) ^ (std::hash<Value*>{}(val) << 3);
    }
};

struct GepHasher
{
    size_t operator()(GepInst* gep, ValueMap* addr) const;
};

class GepEvaluator : public _PassBase<GepEvaluator, Function>
{
    class NodeHandler
    {
        BasicBlock* blk;
        DominantTree* tree;
        DominantTree::TreeNode* node;
        bool done = false;
        std::list<DominantTree::TreeNode*>::iterator childIt;
        std::list<DominantTree::TreeNode*>::iterator childEnd;

    public:
        ValueMap valueMapping;

        NodeHandler(DominantTree* dt, DominantTree::TreeNode* n,
                    std::list<DominantTree::TreeNode*>::iterator begin,
                    std::list<DominantTree::TreeNode*>::iterator end,
                    ValueMap mapping)
            : tree(dt), node(n), childIt(begin), childEnd(end), valueMapping(mapping)
        {
            blk = n->curBlock;
        }

        auto ChildIter() { return childIt; }
        auto NextChildIter() { return childIt++; }
        auto EndIter() { return childEnd; }
        bool isDone() { return done; }
        void markDone() { done = true; }
        BasicBlock* Block() { return blk; }
    };

    Function* function;
    DominantTree* domTree;
    AnalysisManager& analysisMgr;
    std::vector<Instruction*> pendingDelete;
    std::unordered_map<BasicBlock*, NodeHandler*> blockMap;
    std::unordered_map<AllocaInst*, Initializer*> allocInitMap;
    std::unordered_set<AllocaInst*> allocas;

    bool processNode(NodeHandler* node);
    void handleMemcpy(AllocaInst* inst, Initializer* init, NodeHandler* node, std::vector<int> index);
    void handleZeroInit(AllocaInst* inst, NodeHandler* node, std::vector<int> index);
    void processBlock(ValueMap& addr, NodeHandler* node);

public:
    GepEvaluator(Function* f, AnalysisManager& am) : function(f), analysisMgr(am) {}
    bool run();
};
