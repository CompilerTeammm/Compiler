#include "../include/MyBackend/LiveInterval.hpp"

void LiveRange::GetLiveUseAndDef()
{
    for(RISCVBlock* mbb :*curfunc)
    {
        for(RISCVInst* mInst : *mbb)
        {
            for(auto& op : mInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<Register*>(op.get())) 
                {
                    
                }
            }
        }
    }
}


void LiveInterval::orderInsts()
{
    int order = 0;
    for(RISCVBlock* mbb :*curfunc)
    {
        for(RISCVInst* mInst : *mbb)
        {
            RecordInstAndOrder[mInst] = order;
            order++;
        }
    }
}
