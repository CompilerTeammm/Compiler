#pragma once
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "DealOps.hpp"

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

    // flag = 0 ----> int    flag = 1 -----> float
    static ConstantData* ConstFoldBinaryInt(ConstantData*LHS,ConstantData* RHS,int flag=0);
    static ConstantData* ConstFoldBinaryFloat(ConstantData*LHS,ConstantData* RHS,int flag=0);
    static ConstantData* ConstFoldBinaryFloatAndInt(ConstantData*F,ConstantData* I,int flag=0);

    
    ConstantData *ConstFoldCastOps(Instruction *I,
                  ConstantData *op);
};