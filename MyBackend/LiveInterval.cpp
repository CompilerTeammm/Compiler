#include "../include/MyBackend/LiveInterval.hpp"
#include <memory>

void LiveInfo::GetLiveUseAndDef()
{
    for(RISCVBlock* mbb :*curfunc)
    {
        auto& LiveUse = mbb->getLiveUse();
        auto& LiveDef = mbb->getLiveDef();
        for(RISCVInst* mInst : *mbb)
        {
            for(auto& op : mInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<Register*>(op.get()) ) 
                {
                    if (!reg->IsrealRegister())
                    {
                        if (reg == mInst->getOpreand(0).get())
                        {
                            BlockWithVals[reg] = mbb;
                            LiveDef.emplace(reg);
                        }
                        auto it = LiveDef.find(reg);
                        if (LiveDef.find(reg) == LiveDef.end())
                        {
                            LiveUse.emplace(reg);
                        }
                    }
                }
            }
        }
    }
}

void LiveInfo::CalcuLiveInAndOut()
{   
    BlockLiveIn.clear();
    BlockLiveOut.clear();
    for(auto bb = curfunc->rbegin(); bb != curfunc->rend(); --bb)
    {
        BlockLiveIn[*bb] = std::set<Register*>();
        BlockLiveOut[*bb] = std::set<Register*>();
    }

    bool changed;
    do {
        changed = false;

        // 从后到前的遍历顺序
        for (auto bb = curfunc->rbegin(); bb != curfunc->rend(); --bb)
        {
            std::set<Register*> newLiveOut;
            for (auto succbb : (*bb)->getSuccBlocks())
            {
                std::set<Register*> result;
                RISCVBlock *RISCVsuccBB = ctx->mapTrans(succbb)->as<RISCVBlock>();
                std::set_union(
                    newLiveOut.begin(),newLiveOut.end(),
                    BlockLiveIn[RISCVsuccBB].begin(),BlockLiveIn[RISCVsuccBB].end(),
                    std::inserter(result,result.begin())
                );
                newLiveOut = result;
            }
            
            std::set<Register*> newLiveIn;
            std::set_difference(
                newLiveIn.begin(),newLiveIn.end(),
                (*bb)->getLiveDef().begin(),(*bb)->getLiveDef().end(),
                std::inserter(newLiveIn,newLiveIn.begin())
            );

            for(auto reg : (*bb)->getLiveUse()) {
                newLiveIn.insert(reg);
            }

            if (BlockLiveOut[*bb]!= newLiveOut) {
                changed = true;
                BlockLiveOut[*bb] = newLiveOut;
            }

            BlockLiveIn[*bb] = newLiveIn;
        }

    } while (changed);
}

void LiveInterval::orderInsts()
{
    int order = 0;
    for(RISCVBlock* mbb :*curfunc)
    {
        int start = order;
        for(RISCVInst* mInst : *mbb)
        {
            RecordInstAndOrder[mInst] = order;
            order++;
        }
        bbInfos.emplace(mbb,std::make_shared<range>(start,order-1));
    }
}

void LiveInterval::CalcuLiveIntervals()
{
    for (auto bb = curfunc->rbegin(); bb != curfunc->rend(); --bb)
    {
        for(auto [_val,_bb] : liveInfo.BlockWithVals) 
        {
            auto it = bbInfos[*bb];
            regLiveIntervals[_val].emplace_back(std::make_shared<range>(it->start,it->end));
        }

        RISCVBlock* block = *bb;
        RISCVInst *recordInst = nullptr;
        for (auto inst = block->rbegin(); inst != block->rend(); --inst)
        {
            auto defSet = block->getLiveDef();
            auto useSet = block->getLiveUse();
            for(auto op: (*inst)->getOpsVec())
            {
                if(auto reg = dynamic_cast<Register*> (op.get()))
                {
                    if(!reg->IsrealRegister())
                    {

                    }
                }
            }
        }

        // for(auto val :liveInfo.BlockLiveOut[*bb])
        // {
        //     auto it = bbInfos[*bb];
        //     regLiveIntervals[val].emplace_back(std::make_shared<range>(it->start,it->end));
        // }

        // RISCVBlock* block = *bb;

        // int flag = 0;
        // RISCVInst *recordInst = nullptr;
        // for (auto inst = block->rbegin(); inst != block->rend(); --inst)
        // {
        //     auto def = block->getLiveDef();
        //     for(auto op: (*inst)->getOpsVec())
        //     {
        //         if ( Register* use = dynamic_cast<Register*>(op.get()))
        //         {
        //             if (use->RVflag == 0)
        //                 continue;
        //             if(def.find(use) == def.end()) 
        //                 continue;
        //             else {
        //                 recordInst = *inst;
        //                 auto& vec = regLiveIntervals[use];

        //                 auto &vecback = vec.back();
        //                 vecback->start = RecordInstAndOrder[recordInst];
        //                 int a = 10;
        //             }   
        //         }
        //     }

        //     auto useSet = block->getLiveUse();
        //     for(auto op: (*inst)->getOpsVec())
        //     {
        //         if ( Register* use = dynamic_cast<Register*>(op.get()))
        //         {
        //             if (use->RVflag == 0)
        //                 continue;
        //             if (useSet.find(use) == useSet.end())
        //             {
        //                 auto it = bbInfos[*bb];
        //                 regLiveIntervals[use].emplace_back(std::make_shared<range>(it->start,
        //                                                          RecordInstAndOrder[*inst]));
        //             }
        //         }
        //     }
        // }
    }
}

void LiveInterval::FinalCalcu()
{
    for(RISCVBlock* mbb :*curfunc)
    {
        auto& LiveUse = mbb->getLiveUse();
        auto& LiveDef = mbb->getLiveDef();
        for(RISCVInst* mInst : *mbb)
        {
            for(auto& op : mInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<Register*>(op.get()) ) 
                {
                    if (!reg->IsrealRegister())
                    {
                        if (LiveDef.find(reg) == LiveDef.end())
                        {
                            liveInfo.BlockWithVals[reg] = mbb;
                            LiveDef.emplace(reg);
                            regLiveIntervals[reg].emplace_back(std::make_shared<range>(RecordInstAndOrder[mInst]
                                                                                    ,RecordInstAndOrder[mInst]));
                            continue;
                        }
                        if (LiveDef.find(reg) != LiveDef.end())
                        {
                            auto& vec = regLiveIntervals[reg];
                            auto &vecback = vec.back();
                            vecback->end = RecordInstAndOrder[mInst];
                        }

                    }
                }
            }
        }
    }
}

void LiveInterval::run()
{
    // liveInfo.GetLiveUseAndDef();
    // liveInfo.CalcuLiveInAndOut();
    orderInsts();
    // CalcuLiveIntervals();
    FinalCalcu();
}