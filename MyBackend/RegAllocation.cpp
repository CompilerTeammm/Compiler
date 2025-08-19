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

// 理论上这个的实现，可以减轻寄存器分配的压力
// 如果物理寄存器与虚拟寄存器冲突就绝对不能够分配
// 这个我之后再实现它
bool RegAllocation::isConflict(rangeInfoptr rangeInfo)
{
    for (auto& e : interval.getRealRegWithRange())
    {
        if ((rangeInfo->start < e.second->start && rangeInfo->end > e.second->end) ||
            (rangeInfo->start < e.second->end && rangeInfo->end > e.second->end ))
                return true;
    }
    return false;
}

bool RegAllocation::isCallerSavedRR(RealRegister* rr)
{
    return rr->isCallerSaved();
}
bool RegAllocation::isCalleeSavedRR(RealRegister* rr)
{   
    return rr->isCalleeSaved();
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

// fillLinerScaner 填充了 vir 和 real Reg 的 liveInterval
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

    for(auto [reg,val] : interval.getRealRegWithRange()) 
    {
        LinerScaner.emplace_back(reg,val);
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
            // activeRegs.erase(virReg);
            it = active_list.erase(it);
        } else {
            break;  // in order
        }
    }
}

// Todo 溢出未考虑到， LiveReg 需要进行处理
// activeRegs;     map<vir,real> --> 虚拟寄存器分配的实际寄存器
// 如果发生溢出的话，应该有变量被搞出来，需要记录变量在那条语句被溢出了
int RegAllocation::allocateStackLocation(Register* v) 
{
    // 需要判断类型去确定要开辟的栈帧大小, 同一分配 8 字节的大小去作为栈帧槽
    return 8;
}
bool RegAllocation::hasStackSlot(Register* v)
{
    return stackLocation.find(v) != stackLocation.end();
}
void RegAllocation::assignSpill(Register* v)
{
    rangeInfoptr LInfo = nullptr;
    for (auto&[e,LiveInfo] : LinerScaner) {
        if ( v == e) {
            LInfo = LiveInfo;
            break;
        }
    }
    int start = LInfo->start;
    int end = LInfo->end;
    std::vector<RISCVInst*> useInst;
    std::vector<RISCVInst*> defInst;
    for (int tmp = start; tmp != end+1; tmp++)
    {
        RISCVInst* riscvInst = interval.getOrderAndInst()[tmp];
        for (auto& op : riscvInst->getOpsVec()) {
            if (v == op.get()) {
                if (op == riscvInst->getOpreand(0))
                    useInst.push_back(riscvInst);
                else   
                    defInst.push_back(riscvInst);
            }
        }
    }
    
    if (!hasStackSlot(v)) {
        int off = allocateStackLocation(v);
        stackLocation[v] = off;
    }
    spilledRegs.insert(v);
}
// 寄存器的溢出
void RegAllocation::spillInterval(std::pair<Register*,rangeInfoptr> newinterval)
{
    // 选择溢出 end 最大的, 作为溢出的牺牲者
    auto victimIt = std::max_element(active_list.begin(),active_list.end(),
            [](const auto& x,const auto& y) {
                return x.second->end < y.second->end;
            });
    
    if (victimIt != active_list.end() && victimIt->second->end > newinterval.second->end) {
        // 新来的interval 可以不溢出，将 victimIt 筛选出来的进行溢出
        auto victimVReg = victimIt->first;
        auto realReg = activeRegs[victimVReg];

        assignSpill(victimVReg);
        activeRegs.erase(victimVReg);
        victimIt = active_list.erase(victimIt);

        activeRegs[newinterval.first] = realReg;
        active_list.push_back(newinterval);

        // new Interval 的加入，可以使这个重排
        active_list.sort([](const auto& a, const auto& b){ return a.second->end < b.second->end; });
    } else {
        assignSpill(newinterval.first);
    }
}

void RegAllocation::distributeRegs(std::pair<Register*,rangeInfoptr>& newinterval,bool useCalleeSaved)
{   
    RealRegister* reg = nullptr;
    
    if (auto real = dynamic_cast<RealRegister*> (newinterval.first)) {
        if (isFloatReg(newinterval.first))
        {
            reg = real;

            // 加入 active 集合，防止后面错误地复用
            activeRegs[newinterval.first] = reg;
            active_list.emplace_back(newinterval);
            active_list.sort([](const auto &v1, const auto &v2)
                             { return v1.second->end < v2.second->end; });
        }
    } else {   // 虚拟寄存器
        if (isFloatReg(newinterval.first))
        {
            if (useCalleeSaved)
            {
                assert(!calleeSavedFloatPool.empty());
                reg = calleeSavedFloatPool.back();
                calleeSavedFloatPool.pop_back();
                usedCalleeSavedFP.insert(reg);
            }
            else
            {
                assert(!callerSavedFloatPool.empty());
                reg = callerSavedFloatPool.back();
                callerSavedFloatPool.pop_back();
            }
        }
        else
        {
            if (useCalleeSaved)
            {
                assert(!calleeSavedIntPool.empty());
                reg = calleeSavedIntPool.back();
                calleeSavedIntPool.pop_back();
                usedCalleeSavedInt.insert(reg);
            }
            else
            {
                assert(!callerSavedIntPool.empty());
                reg = callerSavedIntPool.back();
                callerSavedIntPool.pop_back();
            }
        }
        activeRegs[newinterval.first] = reg;
        active_list.emplace_back(newinterval);
        active_list.sort([](const auto &v1, const auto &v2)
                         { return v1.second->end < v2.second->end; });
    }
}


void RegAllocation::ScanLiveinterval()
{
    initializeRegisterPool();

    for(auto& e : LinerScaner) 
    {   
        // clear already exit's active
        expireOldIntervals(e);

        auto& callerPool = isFloatReg(e.first) ? callerSavedFloatPool : callerSavedIntPool;
        auto& calleePool = isFloatReg(e.first) ? calleeSavedFloatPool : calleeSavedIntPool;

        const bool isCross = isCrossCall(e.second);   // 判断是否调用跨call Instruction
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
    for (auto& kv : interval.getIntervals()) {
        Register* virReg = kv.first;
        auto itReg = activeRegs.find(virReg);
        auto itSlot = stackLocation.find(virReg);

        if (itReg != activeRegs.end()) {
            virReg->as<VirRegister>()->reWriteRegWithReal(activeRegs[virReg]);
        } else if (itSlot != stackLocation.end()) {
            // 溢出的情况应该有其他的语句去加成
        }
    }

    // callee_saved  caller_saved 重写
    for (auto& kv : CallAndPosRecord) {

    }
}

bool RegAllocation::run()
{
    fillLinerScaner();    // 变量的活跃区间的填充
    ScanLiveinterval();
    ReWriteRegs();

    return true;
}

