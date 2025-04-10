#include "../../include/IR/Opt/DealOps.hpp"

// BinaryInst::Operation Op
// I think only the undef should be deal!!!
// I don't deal the zero 
ConstantData *DealConstType::DealUndefBinary(BinaryInst* inst,ConstantData *LHS, ConstantData *RHS)
{
    BinaryInst::Operation op = inst->GetOp();
    switch (op)
    {
    // calcus +   &&   ||
    case BinaryInst::Op_Add:
    case BinaryInst::Op_Sub:
    case BinaryInst::Op_Mul:
    case BinaryInst::Op_Div:
    case BinaryInst::Op_Mod:
    case BinaryInst::Op_And:
    case BinaryInst::Op_Or:
        return DealUndefCalcu(op,LHS,RHS);
    // cmp
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

// + - *  /  % 
// + -    Undef ----> Undef
// And  
// LHS   1      RHS   2
ConstantData* DealConstType:: DealUndefCalcu(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS)
{
    if(LHS->IsUndefVal() || RHS->IsUndefVal())
    {
        switch (Op)
        {
        case BinaryInst::Op_Add:
        case BinaryInst::Op_Sub:
            return UndefValue::Get(LHS->GetType());
        case BinaryInst::Op_Mul:
            // U * U = U
            if(LHS->IsUndefVal() && RHS->IsUndefVal())
                return LHS;
            else   // X * U -> 0
                return ConstantData::getNullValue(LHS->GetType());
        case BinaryInst::Op_Div:
            // X / U  -> U
            if(RHS->IsUndefVal())
                return RHS;
            // U / 0 -> U    U / 1 -> U
            if(RHS->isConstZero() || RHS->isConstOne())
                return LHS;
            return ConstantData::getNullValue(LHS->GetType());
        case BinaryInst::Op_Mod:
            // X % U -> U
            if(RHS->IsUndefVal())
                return RHS;
            // U % 0 -> U
            if(RHS->isConstZero())
                return LHS;
            // U % X -> 0
            return ConstantData::getNullValue(LHS->GetType());
        case BinaryInst::Op_And:
            // U & U -> U
            if(LHS->IsUndefVal()&& RHS->IsUndefVal())
                return LHS;
            // U & X -> 0
            return ConstantData::getNullValue(LHS->GetType());
        case BinaryInst::Op_Or:
            // X | U -> -1
            // U | U -> U
            if(LHS->IsUndefVal() && RHS->IsUndefVal())
                return LHS;
            // U | X -> ~0   ///  全部置为 U
            return UndefValue::Get(LHS->GetType());
        default:
            return nullptr;
        }
    }
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