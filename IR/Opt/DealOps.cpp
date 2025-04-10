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
        return DealCmp(op,LHS,RHS);
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

// =  !=   >  >=   <   <=
ConstantData* DealCmp(BinaryInst::Operation Op,ConstantData* LHS,ConstantData* RHS)
{
    // if(LHS == RHS && Op == BinaryInst::Op_E)
    //     return ConstIRBoolean::GetNewConstant(true);
    // if(LHS != RHS && Op == BinaryInst::Op_NE)
    //     return ConstIRBoolean::GetNewConstant(true);
    // if(LHS > RHS && Op == BinaryInst::Op_G )
    //     return ConstIRBoolean::GetNewConstant(true);
    return nullptr;
}


// FLAG  ==  0 ---> int   1 ----> float, FLAG 标志该指令是什么类型
ConstantData* DealConstType:: DealIRIntOrFloat(BinaryInst::Operation Op,ConstantData *LHS, ConstantData *RHS, int FLAG)
{
    // 这个判断的是 Ops 操作符的类型
    // INT 的情况
    if (LHS->GetTypeEnum() == IR_Value_INT)
    {
        if (LHS->GetTypeEnum() == IR_Value_INT)
            return ConstFoldBinaryInt(Op,LHS, RHS,FLAG);
        else
            return ConstFoldBinaryFloatAndInt(Op,RHS, LHS,FLAG);
    }
    else
    {
        if (LHS->GetTypeEnum() == IR_Value_Float)
            return ConstFoldBinaryFloat(Op,LHS, RHS,FLAG);
        else
            return ConstFoldBinaryFloatAndInt(Op,LHS, RHS,FLAG);
    }
}

ConstantData* DealConstType::ConstFoldBinaryInt(BinaryInst::Operation Op,ConstantData *LHS, ConstantData *RHS, int flag )
{
    int LVal = dynamic_cast<ConstIRInt*>(LHS)->GetVal();
    int RVal = dynamic_cast<ConstIRInt*>(RHS)->GetVal();

    switch (Op)
    {
        // calcus +   &&   ||
    case BinaryInst::Op_Add:
    case BinaryInst::Op_Sub:
    case BinaryInst::Op_Mul:
    case BinaryInst::Op_Div:
    case BinaryInst::Op_Mod:
    case BinaryInst::Op_And:
    case BinaryInst::Op_Or:
        return ConstFoldInt(Op, LVal, RVal,flag);
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

ConstantData* DealConstType::ConstFoldBinaryFloat(BinaryInst::Operation Op,ConstantData *LHS, ConstantData *RHS, int flag )
{
    return nullptr;
}

ConstantData* DealConstType::ConstFoldBinaryFloatAndInt(BinaryInst::Operation Op, ConstantData *F, ConstantData *I, int flag)
{
    return nullptr;
}


ConstantData* DealConstType::ConstFoldInt(BinaryInst::Operation Op,int LVal,int RVal,int flag)
{
    int Result;
    switch (Op)
    {
    case BinaryInst::Op_Add:
        Result = LVal + RVal;
        break;
    case BinaryInst::Op_Sub:
        Result = LVal - RVal;
        break;
    case BinaryInst::Op_Mul:
        Result = LVal * RVal;
        break;
    case BinaryInst::Op_Div:
        Result = LVal / RVal;
        break;
    case BinaryInst::Op_Mod:
        Result = (LVal % RVal);
        break;
    }

    if (flag == 0)
        return ConstIRInt::GetNewConstant(Result);
    else
        return ConstIRFloat::GetNewConstant(Result);
}
