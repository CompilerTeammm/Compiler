#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/IR/Opt/DealOps.hpp"

ConstantData* ConstantFold:: ConstFoldLoadInst(LoadInst* LI)
{
    return nullptr;
}


ConstantData* ConstantFold::ConstFoldInstruction(Instruction* I)
{
    // handle phiInst here
    // if(auto PInst = dynamic_cast<PhiInst*> (I)){
    //     ConstantData* CommonValue = nullptr;
    //     for(int i = 0; i < PInst->getNumIncomingValues(); i++)
    //     {
    //         Value* Incoming = PInst->getIncomingValue(i);
    //         auto tmp = dynamic_cast<UndefValue*>(Incoming); 
    //         if(tmp)
    //             continue;
            
    //         ConstantData* C = dynamic_cast<ConstantData*> (Incoming);
    //         if(!C)
    //             return nullptr;

    //         // 对phi函数的处理有问题
    //         // C = ConstantFoldInstOperands(C);

    //         if(CommonValue && C!= CommonValue)
    //             return nullptr;
    //         CommonValue = C;
    //     }

    //     return CommonValue ? CommonValue : UndefValue::Get(PInst->GetType());
    // }
    if(dynamic_cast<PhiInst*>(I))
        return nullptr;

    std::vector<ConstantData*> ConstOps;
    // scan the operand list, check to see if they are all constants
    int flag = 0;
    for(int i = 0 ,num = I->GetOperandNums(); i != num ; i++)
    {
        if(!I->GetOperand(i)->isConst())
        {
            flag = 1;
            break;
        }
        ConstOps.push_back(dynamic_cast<ConstantData*>(I->GetOperand(i)));
    }
    if(flag)
        return nullptr;

    if(auto LI = dynamic_cast<LoadInst*>(I))
        return ConstFoldLoadInst(LI);
    
    return ConstantFoldInstOperands(I,ConstOps);
}

ConstantData* ConstantFold:: ConstantFoldInstOperands
                            (Instruction* I,std::vector<ConstantData*>& Ops)
{
    if(I->IsBinary())
        return ConstFoldBinaryOps(I,Ops[0],Ops[1]);
    
    if(I->IsCastInst())
        return ConstFoldCastOps(I,Ops[0]);
    
    if(I->IsGepInst())
        return nullptr;
    
    if(I->IsMaxInst())
        return nullptr;
    
    if(I->IsMinInst())
        return nullptr;
    
    if(I->IsSelectInst())
        return nullptr;

    return nullptr;
}


// need to deal
// Inst: types
//  
// type: int float bool 
// 
ConstantData* ConstantFold::ConstFoldBinaryOps(Instruction* I,
                            ConstantData* LHS,ConstantData* RHS)
{
    BinaryInst* BInst = dynamic_cast<BinaryInst*> (I);
    assert(BInst && "BInst must be BINARY");
    BinaryInst::Operation op = BInst->GetOp();
    ConstantData* ret;

    // Undef 的情况在这里进行定义
    ConstantData* undef = DealConstType::DealUndefBinary(BInst,LHS,RHS);
    if (!undef){
        assert(!dynamic_cast<UndefValue*>(LHS) && !dynamic_cast<UndefValue*>(RHS)
                && "Unexpected UndefValue");
        // without the UndefValue
        // undef == nullptr 

        // SimplyInst also do belowed.
        if (I->GetTypeEnum() == IR_Value_INT){
            ret = DealConstType::DealIROpsIntOrFloat(op,LHS, RHS);
        }
        else if (I->GetTypeEnum() == IR_Value_Float){
            // 处理时Float类型，最后要强转成Float类型
            ret = DealConstType::DealIROpsIntOrFloat(op,LHS, RHS, 1);
        }
        else{
            assert("what happened!!!");
        }
    }
    else 
        return undef;

    return ret;
}

//     Zext,      // 0扩展
//     Sext,  // 符号扩展
//     Trunc, // 截断指令
//     FP2SI, // 浮点到有符号整数， fptosi
//     SI2FP, // 有符号整数到浮点  sitofp
ConstantData *ConstantFold::ConstFoldCastOps(Instruction *I,
                                                 ConstantData *op)
{
    auto Opcode = I->GetInstId();
    Type* ty = I->GetType();

    // deal undef
    // zext(undef) = 0  sext(undef) = 0
    // itofp(undef) = 0
    if(dynamic_cast<UndefValue*>(op)){
        if(Opcode == Instruction::Zext || Opcode == Instruction::Sext 
           || Opcode == Instruction::SI2FP )
            return ConstantData::getNullValue(ty);
        return UndefValue::Get(ty);
    }

    switch (Opcode)
    {
    case Instruction::Zext:
    case Instruction::Sext:
    case Instruction::Trunc:
    case Instruction::FP2SI:
        if(FP2SIInst* FIsnt = dynamic_cast<FP2SIInst*>(I))
        {

        }
    case Instruction::SI2FP:

    default:
        break;
    }

    return nullptr;
}