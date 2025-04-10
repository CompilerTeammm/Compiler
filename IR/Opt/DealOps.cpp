#include "../../include/IR/Opt/DealOps.hpp"

// BinaryInst::Operation Op
// I think only the undef should be deal!!!
// I don't deal the zero 
ConstantData *DealConstType::DealUndefBinary(BinaryInst* inst,ConstantData *LHS, ConstantData *RHS)
{
    switch (inst->GetOp())
    {
    case BinaryInst::Op_Add:
        return DealUndefAdd(LHS,RHS);
    case BinaryInst::Op_Sub:
    case BinaryInst::Op_Mul:
    case BinaryInst::Op_Div:
    case BinaryInst::Op_Mod:

    case BinaryInst::Op_And:
    case BinaryInst::Op_Or:
    case BinaryInst::Op_E:
    case BinaryInst::Op_NE:
    case BinaryInst::Op_G:
    case BinaryInst::Op_GE:
    case BinaryInst::Op_L:
    case BinaryInst::Op_LE:
    
    default:
        return nullptr;
    }
}

ConstantData* DealConstType:: DealUndefAdd(ConstantData* LHS,ConstantData* RHS)
{
    if(LHS->IsUndefVal() || RHS->IsUndefVal())
        return UndefValue::Get(LHS->GetType());
    return nullptr;
}


ConstantData* DealConstType:: DealIRIntAndFloat(ConstantData *LHS, ConstantData *RHS, int FLAG=0)
{
    // INT 的情况
    if (LHS->GetTypeEnum() == IR_Value_INT)
    {
        if (LHS->GetTypeEnum() == IR_Value_INT)
            return ConstantFold::ConstFoldBinaryInt(LHS, RHS);
        else
            return ConstantFold::ConstFoldBinaryFloatAndInt(RHS, LHS);
    }
    else
    {
        if (LHS->GetTypeEnum() == IR_Value_Float)
            return ConstantFold::ConstFoldBinaryFloat(LHS, RHS);
        else
            return ConstantFold::ConstFoldBinaryFloatAndInt(LHS, RHS);
    }
}