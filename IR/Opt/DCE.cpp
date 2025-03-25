#include "../../include/IR/Opt/DCE.hpp"
#include<vector>

// 有副作用
bool DCE::hasSideEffect(Instruction* inst)
{
    

    return false;
}

bool DCE::isInstructionTriviallyDead(Instruction* Inst)
{
    if(!Inst->is_empty() || Inst->IsTerminateInst())
        return false;

    if(hasSideEffect(Inst))
        return false;
    
    return true;
}


bool DCE::IsDCEInstruction(Instruction* I,
                          std::vector<Instruction*> &WorkList)
{
   if(isInstructionTriviallyDead(I))
   {
        //遍历它的op操作数
        for(auto& use : I->GetUserUseList())
        {
            Value* op = use->GetValue();

        }
   }
}

bool DCE::eliminateDeadCode(Function* func)
{
    bool MadeChange = false;
    // 集合
    std::vector<Instruction*> worklist;

    // 遍历每一条语句
    auto BBs = func->GetBBs();
    for(auto& BB : BBs)
    {
        for(auto I = BB->begin(), E = BB->end(); I !=E; ++I)
        {
            if((std::find(worklist.begin(),worklist.end(),I))!= worklist.end())
                MadeChange |= IsDCEInstruction(*I,worklist);
        }
    }

    // IsDceInst function can add new insts, So we need make sure is not empty
    while(!worklist.empty())
    {
        Instruction* Inst = worklist.back();
        worklist.pop_back();
        MadeChange |= IsDCEInstruction(Inst,worklist);
    }

    return MadeChange;
}

void DCE::run()
{
    eliminateDeadCode(_func);
}