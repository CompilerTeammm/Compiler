#include "../../include/IR/Opt/ConstantFold.hpp"

ConstantData* ConstantFold::ConstFoldInstruction(Instruction* I)
{
    if(auto PInst = dynamic_cast<PhiInst*> (I)){

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

}