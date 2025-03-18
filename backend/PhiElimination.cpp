#include "../include/Backend/PhiElimination.hpp"

bool PhiElimination::run(Function *f)
{
  for (auto bb : *f) // ����*fָ���bb����
    runonBasicBlock(bb);
  ///@todo
  return true;
}

void PhiElimination::runonBasicBlock(BasicBlock *bb)
{

  
}