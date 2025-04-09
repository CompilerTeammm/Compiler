#pragma once
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"

class ConstantFold
{
public:
    ConstantData* ConstantFoldBinaryOpOperands
                (Instruction* I,std::vector<ConstantData*>& Ops);
                
    ConstantData* ConstFoldInstruction(Instruction* I);

    ConstantData* ConstFoldLoadInst(LoadInst* LI);
};