#pragma once
#include "../Analysis/LoopInfo.hpp"
// #include "CSE.hpp"

class LoopUnroll : public _PassBase<LoopUnroll, Function>
{
public:
  LoopUnroll(Function *func, AnalysisManager &_AM) : m_func(func), AM(_AM) {}
  bool run();
  ~LoopUnroll()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  CallInst *ExtractLoopBody(LoopInfo *loop);
  BasicBlock *Unroll(LoopInfo *loop, CallInst *UnrollBody);
  BasicBlock *Half_Unroll(LoopInfo *loop, CallInst *UnrollBody);
  bool CanBeFullUnroll(LoopInfo *loop);
  bool CanBeHalfUnroll(LoopInfo *loop);
  int CaculatePrice(const std::vector<BasicBlock *> &body, Function *curfunc,
                    int Iteration = 1);
  void CleanUp(LoopInfo *loop, BasicBlock *clean);
  bool simplify_Block();
  void DeletDeadBlock(BasicBlock *bb);
  Function *m_func;
  DominantTree *dom;
  LoopAnalysis *loopAnaly;
  AnalysisManager &AM;
  std::unordered_map<Value *, Var *> Val2Arg;
  std::vector<LoopInfo *> DeleteLoop;
  Value *OriginChange = nullptr;
  BasicBlock *prehead = nullptr;
  const int MaxInstCost_Before = 200;
  const int MaxInstCost_After = 5000;
  const int MaxInstCost = 2000;
  int HalfUnrollTimes = 0;
};