#include "../../include/IR/Opt/DCE.hpp"
#include <type_traits> 
#include<vector>

bool DCE::IsDCEInstruction()
{

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
            if(!worklist.count(I))
                MadeChange |= IsDCEInstruction();
        }
    }

    while(!worklist.empty())
    {
        Instruction* Inst = worklist.end();
        worklist.pop_back();
        MadeChange |= IsDCEInstruction();
    }

}

void DCE::run()
{

}