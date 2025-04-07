#include "../../include/IR/Opt/DCE.hpp"
#include<vector>

bool DCE::isInstructionTriviallyDead(Instruction* Inst)
{
    if(!Inst->is_empty() || Inst->IsTerminateInst())
        return false;

    if(hasSideEffect(Inst))
        return false;

    return true;
}

// 有副作用
bool DCE::hasSideEffect(Instruction* inst)
{  
    return inst->IsMemoryInst() || inst->IsTerminateInst();
}

bool DCE::DCEInstruction(Instruction* I,
                          std::vector<Instruction*> &WorkList)
{
   if(isInstructionTriviallyDead(I))
   {
        //遍历它的op操作数
        for(int i = 0 , e = I->GetOperandNums(); i!=e ;i++)
        {
            Value* op = I->GetOperand(i);
            I->SetOperand(i, nullptr);

            if(!op->use_empty() || I == op)
                continue;

            if(Instruction* OpI = dynamic_cast<Instruction*>(op))
            {
                if(isInstructionTriviallyDead(OpI))
                    WorkList.push_back(OpI);
            }
        }

        delete I;
        return true;
   }

   return false;
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
                MadeChange |= DCEInstruction(*I,worklist);
        }
    }

    // IsDceInst function can add new insts, So we need make sure is not empty
    while(!worklist.empty())
    {
        Instruction* Inst = worklist.back();
        worklist.pop_back();
        MadeChange |= DCEInstruction(Inst,worklist);
    }

    return MadeChange;
}

void DCE::run()
{
    eliminateDeadCode(_func);
}