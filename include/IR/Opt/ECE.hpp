#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"

class ECE: public _PassBase<ECE, Function> {
private:
  Function *m_func;
  std::set<std::pair<BasicBlock*, BasicBlock*>> splitEdges;
//   _AnalysisManager &AM;

public:
  void SplitCriticalEdges();
  void AddEmptyBlock(Instruction *inst, int succ);
  ECE(Function *func): m_func(func){}
  bool run();
};