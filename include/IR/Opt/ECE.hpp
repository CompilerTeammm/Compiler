#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"

class EdgeCriticalElim : public _PassBase<EdgeCriticalElim, Function> {
private:
    Function* func;
    std::set<std::pair<BasicBlock*, BasicBlock*>> splitEdges;

public:
    explicit EdgeCriticalElim(Function* f) : func(f) {}

    void splitCriticalEdges();
    void insertEmptyBlock(Instruction* inst, int succIndex);

    bool run();
};