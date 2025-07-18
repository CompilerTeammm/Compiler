#include "../include/MyBackend/LiveInterval.hpp"

void LiveRange::GetLiveUseAndDef()
{
    for(RISCVBlock* mbb :*curfunc)
    {
        auto& LiveUse = mbb->getLiveUse();
        auto& LiveDef = mbb->getLiveDef();
        for(RISCVInst* mInst : *mbb)
        {
            for(auto& op : mInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<Register*>(op.get())) 
                {
                    if ( reg == mInst->getOpreand(0).get())
                    {
                        LiveDef.emplace(reg);
                    }
                    auto it = LiveDef.find(reg);
                    if (LiveDef.find(reg) == LiveDef.end()) {
                        LiveUse.emplace(reg);
                    }
                }
            }
        }
    }
}

void LiveRange::CalcuLiveInAndOut()
{
    
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
