#include "../../include/IR/Opt/ConstantFold.hpp"

ConstantData*ConstantFold::ConstFoldLoadInst(LoadInst* LI)
{

}

ConstantData* ConstantFold::ConstFoldInstOperands(ConstantData *I)
{

}

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

            C = ConstFoldInstOperands(C);

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
    
    return ConstantFoldBinaryOpOperands(I,ConstOps);
}

ConstantData* ConstantFold:: ConstantFoldBinaryOpOperands
                            (Instruction* I,std::vector<ConstantData*>& Ops)
{
    if(I->IsBinary())
        return ConstFoldBinaryOps(I,Ops[0],Ops[1]);
    
    // if(I->IsCastInst())
    //     return ConstFoldCastOps(I,Ops[0]);
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
    BinaryInst::Operation op = BInst->GetOp();
    if(op == BinaryInst::Op_Add)
    {
        int Result = dynamic_cast<ConstIRInt*>(LHS)->GetVal() + dynamic_cast<ConstIRInt*>(RHS)->GetVal();
        return ConstIRInt::GetNewConstant(Result);
    }
}