#include "../include/MyBackend/LiveInterval.hpp"
#include <memory>

// #define TEST_BLOCK_INOUT
// #define TEST_LIVEUSE_DEF
// #define TEST_INTERVALS


bool LiveInfo::isDefValue(RISCVInst* mInst)
{
    if(mInst->getOpcode() == RISCVInst::_sd)    return false;
    if(mInst->getOpcode() == RISCVInst::_sw)    return false;
    if(mInst->getOpcode() == RISCVInst::_ble)    return false;
    if(mInst->getOpcode() == RISCVInst::_bge)    return false;
    if(mInst->getOpcode() == RISCVInst::_bgt)    return false;
    if(mInst->getOpcode() == RISCVInst::_bne)    return false;
    if(mInst->getOpcode() == RISCVInst::_blt)    return false;
    if(mInst->getOpcode() == RISCVInst::_beq)    return false;

    return true;
}

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
                    if (reg == mInst->getOpreand(0).get()&& isDefValue(mInst))
                    {
                        DefValsInBlock[reg] = mbb;
                        LiveDef.emplace(reg);
                    }

                    if (LiveDef.find(reg) == LiveDef.end())
                    {
                        LiveUse.emplace(reg);
                    }
                } 
                else if (auto reg = dynamic_cast<RealRegister*> (op.get())) 
                {   
                    if (mInst->getOpcode() != RISCVInst::_sw)
                        RealRegAndInsts.emplace(reg,mInst);
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
    for (auto bb = curfunc->rbegin(); bb != curfunc->rend(); ++bb)
    {
        BlockLiveIn[*bb] = std::set<Register *>();
        BlockLiveOut[*bb] = std::set<Register *>();
    }

    bool changed;
    do {
        changed = false;

        // 从后到前的遍历顺序
        for (auto bb = curfunc->rbegin(); bb != curfunc->rend(); ++bb)
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
    std::unordered_map<RISCVInst*,RealRegister*> riscvInstVec;
    for(auto& [key,val] : liveInfo.RealRegAndInsts)
    {
        realRegWithitRangeInfo[key] = std::make_shared<range> (0,0);
        riscvInstVec.emplace(val,key);
    }

    int order = 0;
    for(RISCVBlock* mbb :*curfunc)
    {
        int start = order;
        for(RISCVInst* mInst : *mbb)
        {
            RecordInstAndOrder[mInst] = order;
            if (riscvInstVec.find(mInst) != riscvInstVec.end())
            {
                RISCVInst* tmp = mInst;
                int tmpOrder = order;
                auto Reg = riscvInstVec[mInst];
                realRegWithitRangeInfo[Reg]->start = order;
                realRegWithitRangeInfo[Reg]->end = order;
                while (1) {
                    tmp = tmp->GetNextNode();
                    if (tmp == nullptr)
                        break;
                    tmpOrder++;
                    if (tmp->getOpcode() == RISCVInst::_ret || tmp->getOpcode() == RISCVInst::_call)
                    {
                        realRegWithitRangeInfo[Reg]->end = tmpOrder;
                        break;
                    }
                }
            }
            if (mInst->getOpcode() == RISCVInst::_call)  // 记录一下call 语句
                CallAndPosRecord[mInst] = order;
            order++;
        }
        bbInfos.emplace(mbb,std::make_shared<range>(start,order-1));
    }
}

void LiveInterval::CalcuLiveIntervals()
{
    for (auto bb = curfunc->rbegin(); bb != curfunc->rend(); --bb)
    {
        RISCVBlock* block = *bb;

        auto& LiveUse = block->getLiveUse();
        auto& LiveDef = block->getLiveDef();
        for(auto& VirReg : liveInfo.BlockLiveOut[block])
        {
            regLiveIntervals[VirReg].push_back(bbInfos[block]);
        }

        if (liveInfo.BlockLiveOut[block].empty())
            goto LABEL;
        for(auto inst = block->rbegin(); inst != block->rend(); --inst)
        {
            RISCVInst* riscvInst = *inst;
            for(auto& op : riscvInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<VirRegister *>(op.get()))
                {
                    if (reg == riscvInst->getOpreand(0).get()&& liveInfo.isDefValue(riscvInst))
                    {
                        auto& range = regLiveIntervals[reg].back();
                        if (regLiveIntervals[reg].empty())
                            break;
                        range->start = RecordInstAndOrder[riscvInst];
                    }

                    if (LiveUse.find(reg) != LiveUse.end())
                    {
                        auto& range = regLiveIntervals[reg].back();
                        if (range == nullptr)
                            break;
                        auto it = std::make_shared<LiveInterval::range>(bbInfos[block]->start,RecordInstAndOrder[riscvInst]);
                        regLiveIntervals[reg].emplace_back(it);
                    }
                }
            }
        }

LABEL:
        // 处理基本块中的虚拟寄存器
        for(RISCVInst* mInst : *block)
        {
            for(auto& op : mInst->getOpsVec())
            {
                if (auto reg = dynamic_cast<VirRegister *>(op.get()))
                {
                    // 不需要再处理BlockLiveOut中的内容
                    if (liveInfo.BlockLiveOut[block].find(reg) != liveInfo.BlockLiveOut[block].end())
                        break;

                    if (reg == mInst->getOpreand(0).get()&&  liveInfo.isDefValue(mInst))
                    {
                        regLiveIntervals[reg].emplace_back(std::make_shared<range>(RecordInstAndOrder[mInst],
                                                                                  RecordInstAndOrder[mInst]));
                    }

                    if (LiveDef.find(reg) != LiveDef.end())
                    {
                        auto &vec = regLiveIntervals[reg];
                        auto &vecback = vec.back();
                        vecback->end = RecordInstAndOrder[mInst];
                    }
                }
            }
        }
    }

#ifdef TEST_INTERVALS
    for(auto& [reg,vec]:regLiveIntervals)
    {
        std::cout << "regName: "<<reg->getName() << std::endl;
        for(auto& e : vec) 
        {
            std::cout << e->start << "-" << e->end << std::endl;
        }
        std::cout << std::endl;
    }    
#endif
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