#pragma once
#include "../../lib/CFG.hpp"
#include "Passbase.hpp"
// #include "PassManager.hpp"


class InlineHeuristic {
public:
    virtual bool CanBeInlined(CallInst* call) = 0;

    static std::unique_ptr<InlineHeuristic> get(Module* m);
};


class InlineHeuristicManager : public InlineHeuristic,
                               public std::vector<std::unique_ptr<InlineHeuristic>> {
public:
    bool CanBeInlined(CallInst* call) override;
    InlineHeuristicManager();
};


class SizeLimit : public InlineHeuristic {
    size_t cost=0;
    const size_t maxframesize = 7864320;
    const size_t maxsize = 10000;
public:
    bool CanBeInlined(CallInst* call) override;
    SizeLimit();
};


class NoRecursive : public InlineHeuristic {
    Module* m;
public:
    bool CanBeInlined(CallInst* call) override;
    NoRecursive(Module* m);
};


class Inliner : public _PassBase<Inliner, Module> {
public:
    bool run();
    bool Inline(Module* m);

    Inliner(Module* m) : m(m) {}

private:
    std::vector<BasicBlock*> CopyBlocks(Instruction* inst);
    void HandleVoidRet(BasicBlock* splitBlock, std::vector<BasicBlock*>& blocks);
    void HandleRetPhi(BasicBlock* RetBlock, PhiInst* phi, std::vector<BasicBlock*>& blocks);

    Module* m;
    std::vector<CallInst*> NeedInlineCall;
    void init(Module* m);
};

