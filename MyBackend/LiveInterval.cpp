#include "../include/MyBackend/LiveInterval.hpp"
#include <memory>

// #define TEST_BLOCK_INOUT
// #define TEST_LIVEUSE_DEF

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
                if (auto reg = dynamic_cast<VirRegister *>(op.get()))
                {
                    if (reg == mInst->getOpreand(0).get()&& (mInst->getOpcode() != RISCVInst::_lw 
                                                            || mInst->getOpcode()!= RISCVInst::_sw))
                    {
                        DefValsInBlock[reg] = mbb;
                        LiveDef.emplace(reg);
                    }

                    if (LiveDef.find(reg) == LiveDef.end())
                    {
                        LiveUse.emplace(reg);
                    }
                }
            }
        }
    }

#ifdef TEST_LIVEUSE_DEF 
    for(RISCVBlock* mbb :*curfunc)
    {
        auto& LiveUse = mbb->getLiveUse();
        auto& LiveDef = mbb->getLiveDef();

        std::cout <<"mbb: " << mbb->getName() << std::endl;
        std::cout << "------ LiveUse -------" << std::endl;
        for (auto& use : LiveUse)
        {
            std::cout << use->getName() << " ";
        }
        std::cout << std::endl;
        std::cout << "------ LiveDef -------" << std::endl;
        for (auto& def : LiveDef)
        {
            std::cout << def->getName() << " ";
        }
        std::cout << std::endl;
    }
#endif
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

#ifdef TEST_BLOCK_INOUT
    std::cout << "-----BlockLiveIn-----"<<std::endl;
    for(auto&[bb,_set] :BlockLiveIn)
    {
        std::cout << bb->getName() << ":" << " ";
        for (auto e : _set) {
            std::cout << e->getName() << "  ";
        }
        std::cout << std::endl;
    }

    std::cout << "-----BlockLiveOut-----"<<std::endl;
    for(auto&[bb,_set] :BlockLiveOut)
    {
        std::cout << bb->getName() << ":" << " ";
        for (auto e : _set) {
            std::cout << e->getName() << "  ";
        }
        std::cout << std::endl;
    }
#endif
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
        for(auto [_val,_bb] : liveInfo.DefValsInBlock) 
        {
            auto it = bbInfos[*bb];
            regLiveIntervals[_val].emplace_back(std::make_shared<range>(it->start,it->end));
        }
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
                    if (!dynamic_cast<RealRegister*>(reg))
                    {
                        if (LiveDef.find(reg) == LiveDef.end())
                        {
                            liveInfo.DefValsInBlock[reg] = mbb;
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
    liveInfo.GetLiveUseAndDef();
    liveInfo.CalcuLiveInAndOut();
    orderInsts();
    CalcuLiveIntervals();

    //FinalCalcu();
}