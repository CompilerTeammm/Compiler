#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/Dominant.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Opt/ConstantFold.hpp"
#include "Passbase.hpp"

class AnalysisManager;
inline void FunctionChange(Function *func)
{
  /*   curfunc->num = 0;
    curfunc->GetBBs().clear();
    for (auto bb : *curfunc)
    {
      bb->num = curfunc->num++;
      curfunc->GetBBs().push_back(std::shared_ptr<BasicBlock>(bb));
    } */
  func->num = 0;

  auto &oldVec = func->GetBBs();

  // 使用 map 保留原来的 shared_ptr，不移动裸指针
  std::unordered_map<BasicBlock *, Function::BBPtr> keep;
  keep.reserve(oldVec.size());
  for (auto &sp : oldVec)
    keep.emplace(sp.get(), sp); // 存 shared_ptr

  // rebuilt vector
  std::vector<Function::BBPtr> rebuilt;
  rebuilt.reserve(oldVec.size());
  for (auto &sp : oldVec)
  {
    sp->num = func->num++;
    rebuilt.push_back(sp);
  }

  oldVec.swap(rebuilt);
}
class LoopRotate : public _PassBase<LoopRotate, Function>
{
public:
  LoopRotate(Function *func, AnalysisManager &_AM) : m_func(func), AM(_AM) {}
  bool run();
  ~LoopRotate()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool RotateLoop(LoopInfo *loop, bool Succ);
  bool TryRotate(LoopInfo *loop);
  bool CanBeMove(Instruction *I);
  void SimplifyBlocks(BasicBlock *Header, LoopInfo *loop);
  void PreservePhi(BasicBlock *header, BasicBlock *Latch, LoopInfo *loop,
                   BasicBlock *preheader, BasicBlock *new_header,
                   std::unordered_map<Value *, Value *> &PreHeaderValue,
                   LoopAnalysis *loopAnlasis);
  void PreserveLcssa(BasicBlock *new_exit, BasicBlock *old_exit,
                     BasicBlock *pred);
  LoopAnalysis *loopAnlasis;
  Function *m_func;
  DominantTree *m_dom;
  AnalysisManager &AM;
  std::unordered_map<Value *, Value *> CloneMap;
  std::vector<LoopInfo *> DeleteLoop;
  const int Heuristic = 8;
};