#pragma once

#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "../Analysis/SideEffect.hpp"

class SelfStoreElimination : public _PassBase<SelfStoreElimination, Function>{
private:
    Function* func;
    DominantTree* tree;
    std::set<User*> wait_del;
    std::vector<BasicBlock*> DFSOrder;
    AnalysisManager &AM;

    void OrderBlock(BasicBlock* bb);
    void CollectStoreInfo(std::unordered_map<Value*, std::vector<User*>>& storeMap);
    void CheckSelfStore(std::unordered_map<Value*, std::vector<User*>>& storeMap);
    void removeInsts();

public:
    SelfStoreElimination(Function* _func, AnalysisManager &_AM) :func(_func),AM(_AM){};
    ~SelfStoreElimination() = default;

    bool run() override;
};