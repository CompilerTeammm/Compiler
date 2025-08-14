#pragma once

#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "../Analysis/SideEffect.hpp"

class SelfStoreElimination : public _PassBase<SelfStoreElimination, Function> {
private:
    Function* func_;
    DominantTree* domTree_;
    SideEffect* sideEffect_;
    std::set<User*> pendingRemoval_;
    std::vector<BasicBlock*> dfsOrder_;
    AnalysisManager& AM_;

    void orderBlocks(BasicBlock* bb);
    void collectStores(std::unordered_map<Value*, std::vector<User*>>& storeMap);
    void identifySelfStores(std::unordered_map<Value*, std::vector<User*>>& storeMap);
    void removeInstructions();

public:
    SelfStoreElimination(Function* func, AnalysisManager& AM)
        : func_(func), AM_(AM) {
        domTree_ = AM_.get<DominantTree>(func_);
        sideEffect_ = AM_.get<SideEffect>(&Singleton<Module>());
    }
    ~SelfStoreElimination() = default;

    bool run() override;
};
