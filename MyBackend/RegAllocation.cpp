#include "../include/MyBackend/RegAllocation.hpp"

// #define INTREGNUM 24
// #define FLOATREGNUM 32

// #define INT_FLOAT_POOL

// int      t2   a0-a7 t3-t6  s1-s11       24
// t0 & t1 will not be regalloced, and it is real temp register
// float  ft0-ft11  fa0-fa7  fs0-fs11      32

bool RegAllocation::isFloatReg(Register *reg)
{
    return reg->IsFloatFlag();
}
bool RegAllocation::isCrossCall(rangeInfoptr rangeInfo)
{
    for (auto& [callInst,pos] : CallAndPosRecord)
    {
        if (rangeInfo->start < pos && rangeInfo->end > pos)
            return true;
    }
    return false;
}
bool RegAllocation::isCallerSavedRR(RealRegister* rr)
{
    rr->isCallerSaved();
}
bool RegAllocation::isCalleeSavedRR(RealRegister* rr)
{   
    rr->isCalleeSaved();
}
void RegAllocation::releaseRealReg(RealRegister* rr,bool isFP)
{
    if (isFP) {
        if (isCalleeSavedRR(rr)) calleeSavedFloatPool.push_back(rr); else callerSavedFloatPool.push_back(rr);
    } else {
        if (isCalleeSavedRR(rr)) calleeSavedIntPool.push_back(rr); else callerSavedIntPool.push_back(rr);
    }
}
void RegAllocation::initializeRegisterPool()
{
    callerSavedIntPool.clear();
    calleeSavedIntPool.clear();
    callerSavedFloatPool.clear();
    calleeSavedFloatPool.clear();

    RegisterVec& vecs = RegisterVec::GetRegVecs();
    callerSavedIntPool = vecs.getIntCaller();
    calleeSavedIntPool = vecs.getIntCallee();
    callerSavedFloatPool = vecs.getFloatCaller();
    calleeSavedFloatPool = vecs.getFloatCallee();

#ifdef INT_FLOAT_POOL
    for (auto&e :RegisterIntpool)
    {
        std::cout << "------- Int Registers--------" << std::endl;
        std::cout << e->getName() << std::endl;
    }

    for (auto&e :RegisterFloatpool)
    {
        std::cout << "------- float Registers--------" << std::endl;
        std::cout << e->getName() << std::endl;
    }
#endif
}

void RegAllocation::fillLinerScaner()
{
    LinerScaner.clear();
    interval.run();
    auto& intervals = interval.getIntervals();
    for(auto [reg,vec] : intervals) 
    {
        if (vec.empty()) 
            continue;

        int final_start = vec.front()->start;
        int final_end = vec.front()->end;

        for(auto& range : vec) {
            final_start = std::min(final_start,range->start);
            final_end = std::max(final_end,range->end);
        }
        LinerScaner.emplace_back(reg,std::make_shared<range>(final_start,final_end));
    }
    // 升序
    std::sort(LinerScaner.begin(), LinerScaner.end(),
              [](const auto &v1, const auto &v2)
              {
                  return v1.second->start < v2.second->start;
              });
    CallAndPosRecord = interval.getCallAndPosRecord();
}


void RegAllocation::expireOldIntervals(std::pair<Register*,LiveInterval::rangeInfoptr> newInterval)
{  
    const int curStart = newInterval.second->start;
    for (auto it = active_list.begin(); it != active_list.end();) {
        if (it->second->end <= curStart) {
            auto virReg = it->first;
            auto realReg = activeRegs[virReg];
            releaseRealReg(realReg,isFloatReg(virReg));
            activeRegs.erase(virReg);
            it = active_list.erase(it);
        } else {
            break;  // in order
        }
    }
}

// Todo 溢出未考虑到， LiveReg 需要进行处理
// activeRegs;     map<vir,real> --> 虚拟寄存器分配的实际寄存器
// 如果发生溢出的话，应该有变量被搞出来，需要记录变量在那条语句被溢出了
int RegAllocation::allocateStackLocation() 
{
    return 0;
}

void RegAllocation::spillInterval(std::pair<Register*,rangeInfoptr> interval)
{
    if(active_list.empty()) {
        assert("needn't to spill");
    }

    auto spill = active_list.back();  // the last is the biggest;
    if(spill.second->end > interval.second->end) {

        activeRegs[interval.first] = activeRegs[spill.first];
        activeRegs.erase(spill.first);

        stackLocation[spill.first] = allocateStackLocation();

        active_list.remove(spill);
        active_list.emplace_back(interval);

        active_list.sort([](const auto &v1, const auto &v2){ 
                            return v1.second->end < v2.second->end; 
                         });
    }
    else {
        stackLocation[interval.first] = allocateStackLocation();
    }
}

void RegAllocation::distributeRegs(std::pair<Register*,rangeInfoptr>& interval,bool useCalleeSaved)
{   
    RealRegister* reg = nullptr;

    if(isFloatReg(interval.first)) {
        if (useCalleeSaved) {
            assert(!calleeSavedFloatPool.empty());
            reg = calleeSavedFloatPool.back();
            calleeSavedFloatPool.pop_back();
            usedCalleeSavedFP.insert(reg);
        } else {
            assert(!callerSavedFloatPool.empty());
            reg = callerSavedFloatPool.back();
            callerSavedFloatPool.pop_back();
        }
    } else {
        if (useCalleeSaved) {
            assert(!calleeSavedIntPool.empty());
            reg = calleeSavedIntPool.back();
            calleeSavedIntPool.pop_back();
            usedCalleeSavedInt.insert(reg);
        } else {
            assert(!callerSavedIntPool.empty());
            reg = callerSavedIntPool.back();
            callerSavedIntPool.pop_back();
        }
    }
    activeRegs[interval.first] = reg;
    active_list.emplace_back(interval);
    active_list.sort([](const auto &v1, const auto &v2){ 
                        return v1.second->end < v2.second->end; 
                    });
}


void RegAllocation::ScanLiveinterval()
{
    initializeRegisterPool();

    for(auto& e : LinerScaner) 
    {   
        // int regNum = getAvailableRegNum(e.first);
        // clear already exit's active
        expireOldIntervals(e);

        auto& callerPool = isFloatReg(e.first) ? callerSavedFloatPool : callerSavedIntPool;
        auto& calleePool = isFloatReg(e.first) ? calleeSavedFloatPool : calleeSavedIntPool;

        const bool isCross = isCrossCall(e.second);
        // 跨调用优先 callee_saved , 非跨调用优先 caller_saved
        if (isCross) {
            if ( !calleePool.empty()) {
                distributeRegs(e,true);   
            } else if (!callerPool.empty()) {
                distributeRegs(e,false);  // caller_saved 但之后需要在调用点插入spill/reload
            } else {
                spillInterval(e);  // spill 没有寄存器可用使用，溢出
            }
        } else {
            if (!callerPool.empty()) {
                distributeRegs(e,false);
            } else if (!calleePool.empty()) {
                distributeRegs(e,true);   // callee_saved ,但是需要 prologue/epilogue 保存
            } else {
                spillInterval(e);  // spill 没有寄存器使用， 溢出
            }
        }
    }
}

// 溢出尚且未考虑到
void RegAllocation::ReWriteRegs()
{
    for(auto [virReg,realReg] : activeRegs)
    {
        virReg->as<VirRegister>()->reWriteRegWithReal(realReg);
    }
}

bool RegAllocation::run()
{
    fillLinerScaner();    // 变量的活跃区间的填充
    ScanLiveinterval();
    ReWriteRegs();

    return true;
}

