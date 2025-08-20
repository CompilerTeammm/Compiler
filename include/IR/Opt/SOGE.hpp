#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
class StoreOnlyGlobalElim : public _PassBase<StoreOnlyGlobalElim, Module> {
public:
    explicit StoreOnlyGlobalElim(Module* m) : module(m) {
        storeOnlyGlobals.clear();
    }

    bool run();

private:
    std::vector<Var*> storeOnlyGlobals;
    Module* module;

    void scanStoreOnlyGlobals();
};