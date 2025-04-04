#include "../../include/IR/Opt/ConstantFold.hpp"

ConstantData* ConstantFold::ConstantFoldBinaryOpOperands
                (int Opcode,Value* LHS,Value* RHS)
{
    Value* newOp;
    switch (Opcode)
    {
    case BinaryInst::Op_Add:
        break;
    
    default:
        break;
    }
}
