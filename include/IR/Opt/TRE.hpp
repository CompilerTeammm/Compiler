#pragma once
// #include "PassManager.hpp"
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"

class TRE : public _PassBase<TRE, Function>{
    Function* func;
    static std::vector<std::pair<CallInst*,RetInst*>> TailCallPairs(Function*);
    std::pair<BasicBlock*,BasicBlock*> HoistAllocas();
    public:
    bool run();
    TRE(Function* _func):func(_func){}
};