#include "../../include/IR/Opt/ConstantFold.hpp"
#include "../../include/IR/Opt/ConstantProp.hpp"

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
        Instruction* I = *WorkList.end();
        WorkList.erase(WorkList.end());

        if(!I->is_empty())
        {
            if(ConstantData* C = FoldManager.ConstFoldInstruction(I))
            {
                for(Use* use : I->GetValUseList())
                {
                    User* user = use->GetUser();
                    WorkList.insert(dynamic_cast<Instruction*>(user));
                }

                I->ReplaceAllUseWith(C);
                WorkList.erase(I);
                if(isInstrutionDead(I)){
                    delete I;
                }
            }
        }
    }
}