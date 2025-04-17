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
        // return DealCmp(op,LHS,RHS);
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
    return nullptr;
}

// =  !=   >  >=   <   <=
template<typename TYPE1,typename TYPE2>
ConstantData*DealConstType:: DealCmp(BinaryInst::Operation Op,TYPE1 LHS,TYPE2 RHS)
{
    if(LHS == RHS && Op == BinaryInst::Op_E)
        return ConstIRBoolean::GetNewConstant(true);
    if(LHS != RHS && Op == BinaryInst::Op_NE)
        return ConstIRBoolean::GetNewConstant(true);
    if(LHS > RHS && Op == BinaryInst::Op_G )
        return ConstIRBoolean::GetNewConstant(true);
    if(LHS >= RHS && Op == BinaryInst::Op_GE)
        return ConstIRBoolean::GetNewConstant(true);
    if(LHS < RHS && Op == BinaryInst::Op_L)
        return ConstIRBoolean::GetNewConstant(true);
    if(LHS <= RHS && Op == BinaryInst::Op_LE)
        return ConstIRBoolean::GetNewConstant(true);

    return ConstIRBoolean::GetNewConstant(false);
}


// FLAG  ==  0 ---> int   1 ----> float, FLAG 标志该指令是什么类型
ConstantData* DealConstType:: DealIROpsIntOrFloat(BinaryInst::Operation Op,ConstantData *LHS, ConstantData *RHS, int FLAG)
{
    // int LVal = dynamic_cast<ConstIRInt*>(LHS)->GetVal();
    // int RVal = dynamic_cast<ConstIRInt*>(RHS)->GetVal();
    // float a = dynamic_cast<ConstIRFloat*>(LHS)->GetVal();
    // float b = dynamic_cast<ConstIRFloat*>(RHS)->GetVal();
    // if(dynamic_cast<ConstIRInt*>(LHS))
    int LFlag = 0, RFlag = 0;
    if(LHS->GetTypeEnum() == IR_Value_Float)
        LFlag = 1;
    if(RHS->GetTypeEnum() == IR_Value_Float)
        RFlag = 1;

    switch (Op)
    {
    // calcus +   &&   ||
    case BinaryInst::Op_Add:
    case BinaryInst::Op_Sub:
    case BinaryInst::Op_Mul:
    case BinaryInst::Op_Div:
    case BinaryInst::Op_Mod:

        if (LFlag == 0 && RFlag == 0)
            return ConstFoldCaclu<int, int>(Op, dynamic_cast<ConstIRInt *>(LHS)->GetVal(),
                                            dynamic_cast<ConstIRInt *>(RHS)->GetVal(), FLAG);
        if (LFlag == 0 && RFlag == 1)
            return ConstFoldCaclu<int, float>(Op, dynamic_cast<ConstIRInt *>(LHS)->GetVal(),
                                              dynamic_cast<ConstIRFloat *>(RHS)->GetVal(), FLAG);
        if (LFlag == 1 && RFlag == 0)
            return ConstFoldCaclu<float, int>(Op, dynamic_cast<ConstIRFloat *>(LHS)->GetVal(),
                                              dynamic_cast<ConstIRInt *>(RHS)->GetVal(), FLAG);
        if (LFlag == 1 && RFlag == 1)
            return ConstFoldCaclu<float, float>(Op, dynamic_cast<ConstIRFloat *>(LHS)->GetVal(),
                                                dynamic_cast<ConstIRFloat *>(RHS)->GetVal(), FLAG);

    // cmp
    case BinaryInst::Op_E:
    case BinaryInst::Op_NE:
    case BinaryInst::Op_G:
    case BinaryInst::Op_GE:
    case BinaryInst::Op_L:
    case BinaryInst::Op_LE:
        if (dynamic_cast<ConstIRBoolean *>(LHS) || dynamic_cast<ConstIRBoolean *>(RHS))
        {
            return DealCmp<bool,bool>(Op,dynamic_cast<ConstIRBoolean *>(LHS)->GetVal(),
                                         dynamic_cast<ConstIRBoolean *>(RHS)->GetVal());
        }
        else
        {
            if (LFlag == 0 && RFlag == 0)
                return DealCmp<int, int>(Op, dynamic_cast<ConstIRInt *>(LHS)->GetVal(),
                                         dynamic_cast<ConstIRInt *>(RHS)->GetVal());
            if (LFlag == 0 && RFlag == 1)
                return DealCmp<int, float>(Op, dynamic_cast<ConstIRInt *>(LHS)->GetVal(),
                                           dynamic_cast<ConstIRFloat *>(RHS)->GetVal());
            if (LFlag == 1 && RFlag == 0)
                return DealCmp<float, int>(Op, dynamic_cast<ConstIRFloat *>(LHS)->GetVal(),
                                           dynamic_cast<ConstIRInt *>(RHS)->GetVal());
            if (LFlag == 1 && RFlag == 1)
                return DealCmp<float, float>(Op, dynamic_cast<ConstIRFloat *>(LHS)->GetVal(),
                                             dynamic_cast<ConstIRFloat *>(RHS)->GetVal());
        }
    case BinaryInst::Op_And:
    case BinaryInst::Op_Or:
    default:
        return nullptr;
    }
}

template<typename TYPE1,typename TYPE2>
ConstantData* DealConstType::ConstFoldCaclu(BinaryInst::Operation Op,TYPE1 LVal,TYPE2 RVal,int flag)
{
    if (flag == 0)
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
            Result = int(LVal) % int(RVal);
            break;
        default:
            assert("can't deal the Fold");
        }

        return ConstIRInt::GetNewConstant(Result);
    }
    else 
    {
        float Result;
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
        // case BinaryInst::Op_Mod:
        //     Result = (LVal % RVal);
        //     break;
        default:
            assert("can't deal the Fold");
        }

        return ConstIRFloat::GetNewConstant(Result);
    }
}


// 使用template 简化判断的过程
    // 这个判断的是 Ops 操作符的类型
    // INT 的情况
    // if (LHS->GetTypeEnum() == IR_Value_INT)
    // {
    //     if (LHS->GetTypeEnum() == IR_Value_INT)
    //         return ConstFoldBinaryInt(Op,LHS, RHS,FLAG);
    //     else
    //         return ConstFoldBinaryFloatAndInt(Op,RHS, LHS,FLAG);
    // }
    // else
    // {
    //     if (LHS->GetTypeEnum() == IR_Value_Float)
    //         return ConstFoldBinaryFloat(Op,LHS, RHS,FLAG);
    //     else
    //         return ConstFoldBinaryFloatAndInt(Op,LHS, RHS,FLAG);
    // }