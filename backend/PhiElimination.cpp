#include "../include/Backend/PhiElimination.hpp"

bool PhiElimination::run(Function *f)
{
  for (auto bb : *f) // 遍历*f指向的bb类型
    runonBasicBlock(bb);
  ///@todo
  return true;
}

void PhiElimination::runonBasicBlock(BasicBlock *bb)
{
}