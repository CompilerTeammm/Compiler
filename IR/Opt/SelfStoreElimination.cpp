#include "../../include/IR/Opt/SelfStoreElimination.hpp"

bool SelfStoreElimination::run() {
    wait_del.clear();
    func->ClearInlineInfo

    OrderBlock(func->front());

    std::reverse(DFSOrder.begin(), DFSOrder.end());

    std::unordered_map<Value*, std::vector<StoreInst*>> storeMap;
    CollectStoreInfo(storeMap);
    EliminateRedundantStores(storeMap);
    removeInsts();

    return !wait_del.empty();
}