#pragma once
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "DealUndefOps.hpp"
class ConstantFold
{
public:
    ConstantData* ConstFoldInstruction(Instruction* I);

    ConstantData* ConstantFoldInstOperands
                (Instruction* I,std::vector<ConstantData*>& Ops);

    ConstantData* ConstFoldLoadInst(LoadInst* LI);

    // BinaryOps Int Float Boolen
    ConstantData* ConstFoldBinaryOps(Instruction* I,
                  ConstantData* LHS,ConstantData* RHS);
    ConstantData* ConstFoldBinaryInt(ConstantData*LHS,ConstantData* RHS);
    
    ConstantData *ConstFoldCastOps(Instruction *I,
                  ConstantData *op);
};