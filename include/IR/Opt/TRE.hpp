#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"

class TailRecElim : public _PassBase<TailRecElim, Function> {
    Function* func;

    std::pair<BasicBlock*, BasicBlock*> liftAllocas();
    static std::vector<std::pair<CallInst*, RetInst*>> collectTailCalls(Function* f);

public:
    explicit TailRecElim(Function* f) : func(f) {}
    bool run();
};
