#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/IR/Opt/DealOps.hpp"

ConstantData* ConstantFold::ConstFoldInstruction(Instruction* I)
{
    // handle phiInst here
    if(auto PInst = dynamic_cast<PhiInst*> (I)){
        ConstantData* CommonValue = nullptr;
        for(int i = 0; i < PInst->getNumIncomingValues(); i++)
        {
            Value* Incoming = PInst->getIncomingValue(i);
            if(dynamic_cast<UndefValue*>(Incoming))
                continue;
            
            ConstantData* C = dynamic_cast<ConstantData*> (Incoming);
            if(!C)
                return nullptr;

            // 对phi函数的处理有问题
            C = ConstantFoldInstOperands(C);

            if(CommonValue && C!= CommonValue)
                return nullptr;
            CommonValue = C;
        }

        return CommonValue ? CommonValue : UndefValue::Get(PInst->GetType());
    }

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
    
    // Undef 的情况在这里进行定义
    ConstantData* undef = DealConstType::DealUndefBinary(BInst,LHS,RHS);
    if (!undef){
        // undef == nullptr 
        if (I->GetTypeEnum() == IR_Value_INT){
            DealConstType::DealIRIntAndFloat(LHS, RHS);
        }
        else if (I->GetTypeEnum() == IR_Value_Float){
            // 处理时Float类型，最后要强转成Float类型
            DealConstType::DealIRIntAndFloat(LHS, RHS, 1);
        }
        else{
            assert("what happened!!!");
        }
    }
    else 
        return undef;

    return nullptr;
}

ConstantData*ConstantFold::ConstFoldLoadInst(LoadInst* LI)
{

}
