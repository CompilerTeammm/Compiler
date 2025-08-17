#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/SideEffect.hpp"
#include "Passbase.hpp"
#include <cmath>
// #include "lcssa.hpp"
// #include "licm.hpp"

class AnalysisManager;

class ScalarStrengthReduce : public _PassBase<ScalarStrengthReduce, Function>
{
  private:
    Function *func;
    DominantTree *DomTree;
    LoopAnalysis *Loop;
    AnalysisManager &AM;
    std::vector<LoopInfo *> DeleteLoop;
    std::vector<LoopInfo*> Loops;
    void init();
    bool RunOnLoop(LoopInfo* loop);
    Value* CaculateTimes(LoopInfo* loop);
  public:
    ScalarStrengthReduce(Function *func_, AnalysisManager &AM_) : func(func_), AM(AM_)
    {
        init();
    }
    ~ScalarStrengthReduce()
    {
        for (auto Loop : DeleteLoop)
            delete Loop;
    }
    bool run();
};