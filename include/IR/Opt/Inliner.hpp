#pragma once
#include "../../lib/CFG.hpp"
#include "Passbase.hpp"

class InlinePolicy
{
public:
    virtual ~InlinePolicy() = default;
    virtual bool shouldInline(CallInst *call) = 0;

    static std::unique_ptr<InlinePolicy> create(Module *module);
};

class PolicySet : public InlinePolicy
{
public:
    PolicySet();

    bool shouldInline(CallInst *call) override;

    void addPolicy(std::unique_ptr<InlinePolicy> policy);

private:
    std::vector<std::unique_ptr<InlinePolicy>> policies;
};

/// 限制函数体大小的策略
class BudgetPolicy final : public InlinePolicy
{
public:
    BudgetPolicy();

    bool shouldInline(CallInst *call) override;

private:
    size_t currentCost = 0;
    static constexpr size_t MaxFrameSize = 7864320; // ~7.5MB
    static constexpr size_t MaxInstCount = 10000;
};

/// 禁止递归内联的策略
class NoSelfCall final : public InlinePolicy
{
public:
    explicit NoSelfCall(Module *module);

    bool shouldInline(CallInst *call) override;

private:
    Module *mod;
};

class Inliner : public _PassBase<Inliner, Module>
{
public:
    explicit Inliner(Module *module): mod(module) {}

    bool run();
    bool performInline(Module *module);

private:
    void initialize(Module *module);

    std::vector<BasicBlock *> cloneBlocks(Instruction *inst);
    void fixVoidReturn(BasicBlock *splitBlock, std::vector<BasicBlock *> &blocks);
    void fixReturnPhi(BasicBlock *retBlock, PhiInst *phi, std::vector<BasicBlock *> &blocks);

private:
    Module *mod;
    std::vector<CallInst *> pendingCalls;
};