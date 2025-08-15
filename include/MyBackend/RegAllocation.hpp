#pragma once
#include "BackendPass.hpp"
#include "LiveInterval.hpp"
#include "MIR.hpp"
#include <algorithm>
#include "RegisterVec.hpp"

// Regester Allocation (RA)
// I chose the LINEARSCANREGISTERALLOCATION 
class RegAllocation :public BackendPassBase
{
    using range = LiveInterval::range;
    using rangeInfoptr = LiveInterval::rangeInfoptr;
    std::shared_ptr<RISCVContext> ctx;
    RISCVFunction* mfunc;
    LiveInterval interval;

    std::vector<std::pair<Register*,LiveInterval::rangeInfoptr>> LinerScaner; // 线性输入，升序
    std::list<std::pair<Register*,LiveInterval::rangeInfoptr>> active_list;   // 活跃集合

    // int/float   caller/callee
    std::vector<RealRegister*> callerSavedIntPool;                   // realReg  
    std::vector<RealRegister*> calleeSavedIntPool;                     
    std::vector<RealRegister*> callerSavedFloatPool;
    std::vector<RealRegister*> calleeSavedFloatPool;

    // prepare for scanLine
    std::map<Register*,RealRegister*> activeRegs;           // map<vir,real> --> 虚拟寄存器分配的实际寄存器
    std::unordered_map<Register*,int> stackLocation;    // map<vir, offset>
    std::unordered_set<Register*> spilledRegs;   // 记录溢出寄存器

    using position = int;
    std::map<RISCVInst*,position> CallAndPosRecord;

    // 记录已使用的 callee_saved 寄存器，用于 pro epi 保存/恢复
    std::unordered_set<RealRegister*> usedCalleeSavedInt;
    std::unordered_set<RealRegister*> usedCalleeSavedFP;
public:
    RegAllocation(RISCVFunction* _mfunc,std::shared_ptr<RISCVContext>& _ctx)
                :mfunc(_mfunc),ctx(_ctx),interval(_mfunc,_ctx) { }
    bool run() override; 
    void fillLinerScaner();
    void ScanLiveinterval();
    void expireOldIntervals(std::pair<Register*,rangeInfoptr> newInterval);

    // spill funcs
    bool hasStackSlot(Register* v);
    void assignSpill(Register* v);
    int allocateStackLocation(Register* v);
    void spillInterval(std::pair<Register*,rangeInfoptr> );


    void distributeRegs(std::pair<Register*,rangeInfoptr>& interval,bool useCalleeSaved);
    bool isFloatReg(Register* reg);
    //int getAvailableRegNum(Register* reg);
    void initializeRegisterPool();
    void ReWriteRegs();

    // deal Call
    bool isCrossCall(rangeInfoptr rangeInfo);
    bool isCallerSavedRR(RealRegister* rr);
    bool isCalleeSavedRR(RealRegister* rr);
    void releaseRealReg(RealRegister* rr,bool isFP);
};

