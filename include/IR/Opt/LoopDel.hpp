#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Opt/ConstantFold.hpp"
#include "Passbase.hpp"

class AnalysisManager;
class LoopDeletion : public _PassBase<LoopDeletion, Function> {
public:
  LoopDeletion(Function *m_func, AnalysisManager &_AM)
      : func(m_func), AM(_AM) {}
  bool run();
  ~LoopDeletion() {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool DetectDeadLoop(LoopInfo *loop);
  bool TryDeleteLoop(LoopInfo *loop);
  bool CanBeDelete(LoopInfo *loop);
  bool IsSafeToMove(Instruction *inst);
  bool makeLoopInvariant(Instruction *inst, LoopInfo *loop, Instruction *Termination);
  bool makeLoopInvariant_val(Value *val, LoopInfo *loop, Instruction *Termination);
  bool isLoopInvariant(std::set<BasicBlock *> &blocks, Instruction *inst);
  void DeletDeadBlock(BasicBlock *bb);
  void updateDTinfo(BasicBlock *bb);
  bool DeleteUnReachable();
private:
  LoopAnalysis *loop;
  Function *func;
  DominantTree *dom;
  AnalysisManager &AM;
  std::vector<LoopInfo *> DeleteLoop;
};
