#pragma once
#include <vector>
#include <memory>
#include "../../lib/CFG.hpp"



class InlineHeuristic {
public:
    virtual ~InlineHeuristic() = default;
    virtual bool CanBeInlined(CallInst* call) = 0;

    static std::unique_ptr<InlineHeuristic> CreateDefaultHeuristic(Module* m);
};


class InlineHeuristicManager : public InlineHeuristic {
public:
    explicit InlineHeuristicManager(Module* m);
    bool CanBeInlined(CallInst* call) override;

private:
    std::vector<std::unique_ptr<InlineHeuristic>> heuristics_;
};


class SizeLimitHeuristic : public InlineHeuristic {
public:
    SizeLimitHeuristic(size_t maxSize = 10000);
    bool CanBeInlined(CallInst* call) override;

private:
    size_t maxSize_;
};


class NoRecursiveHeuristic : public InlineHeuristic {
public:
    explicit NoRecursiveHeuristic(Module* m);
    bool CanBeInlined(CallInst* call) override;

private:
    Module* module_;
};


class Inliner {
public:
    explicit Inliner(Module* m);

    // 运行内联优化，返回是否有改动
    bool Run();

private:
    bool InlineCall(CallInst* call);
    std::vector<BasicBlock*> CopyBlocks(User* inst);
    void HandleVoidRet(BasicBlock* splitBlock, std::vector<BasicBlock*>& blocks);
    void HandleRetPhi(BasicBlock* retBlock, PhiInst* phi, std::vector<BasicBlock*>& blocks);

private:
    Module* module_;
    std::unique_ptr<InlineHeuristic> heuristic_;

    // 待内联调用指令列表
    std::vector<CallInst*> callsToInline_;
};
