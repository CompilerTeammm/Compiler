#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"

class _AnalysisManager;

class ConstantHoist : public _PassBase<ConstantHoist, Function>
{
  struct HoistNode
  {
    Instruction *LHS_Inst;
    Value *LHS;
    Instruction *RHS_Inst;
    Value *RHS;
    int index;
    HoistNode(Instruction *LHS_Inst_, Value *LHS_, Instruction *RHS_Inst_, Value *RHS_, int index_)
        : LHS_Inst(LHS_Inst_), LHS(LHS_), RHS_Inst(RHS_Inst_), RHS(RHS_), index(index_)
    {
    }
  };

private:
  Function *func;
  DominantTree *DomTree;
  //_AnalysisManager &AM;
  std::vector<HoistNode *> HoistList;
  std::unordered_map<BasicBlock *, std::unordered_map<Instruction *, int>> InstIndex;

  bool RunOnBlock(BasicBlock *bb);
  bool HoistInstInBlock(BasicBlock *TrueBlock, BasicBlock *FalseBlock);

public:
  ConstantHoist(Function *func_) : func(func_)
  {
  }
  ~ConstantHoist()
  {
    HoistList.clear();
  }
  bool run();
};