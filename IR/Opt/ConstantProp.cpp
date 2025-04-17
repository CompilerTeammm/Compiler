#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/IR/Opt/ConstantProp.hpp"

// 之后可能会把run()-> void -----> run() -> bool
void ConstantProp::run()
{
    std::set<Instruction*> WorkList;
    for(BasicBlock* BB : *_func)
    {
        for(auto I = BB->begin(), E = BB->end();I!=E; ++I)
        {   
            WorkList.insert(*I);
        }
    }

    while(!WorkList.empty())
    {
        Instruction* I = *WorkList.begin();
        WorkList.erase(WorkList.begin());

        if(!I->is_empty())
        {
            if(ConstantData* C = FoldManager->ConstFoldInstruction(I))
            {
                for(Use* use : I->GetValUseList())
                {
                    User* user = use->GetUser();
                    Instruction* inst = dynamic_cast<Instruction*>(user);
                    if(inst)
                        WorkList.insert(inst);
                }

                I->ReplaceAllUseWith(C);
                WorkList.erase(I);
                if(DCE::isInstructionTriviallyDead(I)){
                    delete I;
                }
            }
        }
    }
}