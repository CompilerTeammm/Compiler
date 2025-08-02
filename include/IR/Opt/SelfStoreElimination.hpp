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

    void OrderBlock(BasicBlock* bb);
    void CollectStoreInfo(std::unordered_map<Value*, std::vector<StoreInst*>>& storeMap);
    void EliminateRedundantStores(std::unordered_map<Value*, std::vector<StoreInst*>>& storeMap);
    void removeInsts();

public:
    SelfStoreElimination(Function* _func, DominantTree* _tree) :func(_func),tree(_tree){};
    ~SelfStoreElimination() = default;

    bool run() override;
};