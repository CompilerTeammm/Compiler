#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/Dominant.hpp"
#include "PassManager.hpp"
#include "Passbase.hpp"
#include <unordered_map>
#include <vector>

class LoopRotaing : public _PassBase<LoopRotaing, Function>
{
public:
  LoopRotaing(Function *func, PassManager &_AM) : m_func(func), AM(_AM) {}
  bool Run();
  void PrintPass();
  ~LoopRotaing()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool RotateLoop(Loop *loop, bool Succ);
  bool TryRotate(Loop *loop);
  bool CanBeMove(User *I);
  void SimplifyBlocks(BasicBlock *Header, Loop *loop);
  void PreservePhi(BasicBlock *header, BasicBlock *Latch, Loop *loop,
                   BasicBlock *preheader, BasicBlock *new_header,
                   std::unordered_map<Value *, Value *> &PreHeaderValue,
                   LoopInfoAnalysis *loopAnlasis);
  void PreserveLcssa(BasicBlock *new_exit, BasicBlock *old_exit,
                     BasicBlock *pred);
  LoopInfoAnalysis *loopAnlasis;
  Function *m_func;
  DominantTree *m_dom;
  PassManager &AM;
  std::unordered_map<Value *, Value *> CloneMap;
  std::vector<Loop *> DeleteLoop;
  const int Heuristic = 8;
};