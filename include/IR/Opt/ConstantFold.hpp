#pragma once
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"

class ConstantFold
{
public:
    ConstantData* ConstantFoldBinaryOpOperands
                (int Opcode,Value* LHS,Value* RHS);
};