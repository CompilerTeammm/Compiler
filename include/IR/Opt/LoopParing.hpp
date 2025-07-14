#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "AnalysisManager.hpp"
#include "Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"

class LoopParing : public _PassBase<LoopParing, Function>
{
public:
  LoopParing(Function *func, AnalysisManager &am) : _func(func), _analysis(am) {}
  bool run();
  ~LoopParing() {}

private:
  bool CanBeParallel(Loop *loop);
  bool every_LoopParallelBody(Loop *loop);
  void evert_thread(Value *_initial, Value *_boundary, bool every_call);
  bool makeit(std::vector<Loop *> loops);
  void deletephi(Function *_func);

private:
  Function *_func;
  AnalysisManager &_analysis;
  std::vector<Loop *> DeleteLoop;
  DominantTree *_dominatortree;
  LoopInfoAnalysis *_loop_analysis;
  std::unordered_set<BasicBlock *> processed_Loops;
  LoopTrait every_new_looptrait;
};