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
    explicit InlineHeuristicManager(Module* m) : module_(m) {}
    
    bool CanBeInlined(CallInst* call) override {
        for (auto& heuristic : heuristics_) {
            if (!heuristic->CanBeInlined(call)) {
                return false;
            }
        }
        return true;
    }

    void push_back(std::unique_ptr<InlineHeuristic> h) {
        heuristics_.push_back(std::move(h));
    }

private:
    std::vector<std::unique_ptr<InlineHeuristic>> heuristics_;
    Module* module_;
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

    bool Run();

    bool Inline(Module* m);
    void init(Module* m);
    std::pair<int, BinaryInst::Operation> MatchLib(Function* func);

private:
    bool InlineCall(CallInst* call);

    std::vector<BasicBlock*> CopyBlocks(User* callSite);

    void HandleVoidRet(BasicBlock* splitBlock, std::vector<BasicBlock*>& blocks);

    void HandleRetPhi(BasicBlock* retBlock, PhiInst* phi, std::vector<BasicBlock*>& blocks);

private:
    Module* module_;
    std::vector<CallInst*> callsToInline_;
    std::unique_ptr<InlineHeuristic> heuristic_;
};

