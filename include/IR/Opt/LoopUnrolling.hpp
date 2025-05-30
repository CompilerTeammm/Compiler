#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "AnalysisManager.hpp"
#include "../Analysis/LoopInfo.hpp"

class LoopUnrolling : public _PassBase<LoopUnrolling, Function>
{
public:
  LoopUnrolling(Function *func, AnalysisManager *AM) : _AM(AM), _func(func) {}
  bool run();

  ~LoopUnrolling()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  Function *_func;
  AnalysisManager *_AM;
  DominantTree *_dom;
  LoopInfoAnalysis *loopAnalysis;
  std::vector<Loop *> DeleteLoop;
  const int MaxInstCost_Before = 200;
  const int MaxInstCost = 2000;
  const int MaxInstCost_After = 5000;
  Value *Origin = nullptr;
  std::unordered_map<Value *, Var *> Val2Arg;
  bool CanBeUnroll(Loop *loop);
  CallInst *GetLoopBody(Loop *loop);
  BasicBlock *Unroll(Loop *loop, CallInst *UnrollBody);
  int CaculatePrice(std::vector<BasicBlock *> body, Function *curfunc, int Lit_count);
};
