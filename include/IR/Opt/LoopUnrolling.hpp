#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "AnalysisManager.hpp"
#include "../Analysis/LoopInfo.hpp"

class LoopUnrolling : public _PassBase<LoopUnrolling, Function>
{
public:
  LoopUnrolling(Function *func, AnalysisManager &AM) : _func(func), _AM(AM) {}
  bool run() override
  {
    // 调用您现有的 Run() 逻辑，或直接实现功能
    return Run(); // 如果已有 Run() 的实现
  }

  ~LoopUnrolling()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool Run();
  Function *_func;
  AnalysisManager &_AM;
  DominantTree *_dom;
  LoopInfoAnalysis *loopAnalysis;
  std::vector<Loop *> DeleteLoop;
  BasicBlock *prehead = nullptr;
  const int MaxInstCost_Before = 200;
  const int MaxInstCost = 2000;
  const int MaxInstCost_After = 5000;
  Value *Origin = nullptr;
  std::unordered_map<Value *, Var *> Val2Arg;
  bool CanBeUnroll(Loop *loop, DominantTree *_dom);
  CallInst *GetLoopBody(Loop *loop);
  BasicBlock *Unroll(Loop *loop, CallInst *UnrollBody);
  BasicBlock *Half_Unroll(Loop *loop, CallInst *UnrollBody);
  int CaculatePrice(std::vector<BasicBlock *> body, Function *curfunc, int Lit_count);
  void CleanUp(Loop *loop, BasicBlock *clean);
  bool CanBeHalfUnroll(Loop *loop, LoopInfoAnalysis *loopAnalysis, DominantTree *_dom);
  Value *OriginChange = nullptr;
  int HalfUnrollTimes = 0;
};
