#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/IR/Opt/ConstantProp.hpp"

void ConstantProp::run()
{
    for(BasicBlock* BB : *_func)
    {
        for(auto I = BB->begin(), E = BB->end();I!=E; ++I)
        {   
            Instruction* inst = *I;
            if(BinaryInst* BInst = dynamic_cast<BinaryInst*>(inst))
            {
                Value* op1 = BInst->GetOperand(0);
                Value* op2 = BInst->GetOperand(1);
                FoldManager.ConstantFoldBinaryOpOperands(BInst->GetOp(),op1,op2);
            }
        }
    }
}