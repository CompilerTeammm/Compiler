#include "../include/MyBackend/RISCVSelect.hpp"


void RISCVSelect:: MatchAllInsts(BasicBlock* bb)
{
    // 取所有的Insts
    for(auto it = bb->begin(),LInst = bb->end(); 
                                                 it!= LInst ; ++it)
    {
        Instruction* Inst = *it;
        ctx->mapTrans(Inst);
    }
}
