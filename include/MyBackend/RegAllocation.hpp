#pragma once
#include "BackendPass.hpp"
#include "LiveInterval.hpp"
#include "MIR.hpp"
#include <algorithm>
#include "RegisterVec.hpp"

// Regester Allocation (RA)
// I choose the LINEARSCANREGISTERALLOCATION 
class RegAllocation :public BackendPassBase
{
    using range = LiveInterval::range;
    using rangeInfoptr = LiveInterval::rangeInfoptr;
    std::shared_ptr<RISCVContext> ctx;
    RISCVFunction* mfunc;
    LiveInterval interval;
    std::vector<std::pair<Register*,LiveInterval::rangeInfoptr>> LinerScaner;
    std::list<std::pair<Register*,LiveInterval::rangeInfoptr>> active_list;
    std::vector<RealRegister*> RegisterIntpool;                    // realReg  
    std::vector<RealRegister*> RegisterFloatpool;                  // realReg  
    std::map<Register*,Register*> activeRegs;           // map<vir,real> --> 虚拟寄存器分配的实际寄存器
    std::unordered_map<Register*,int> stackLocation;    // map<vir, offset>

public:
    bool run() override; 
    RegAllocation(RISCVFunction* _mfunc,std::shared_ptr<RISCVContext>& _ctx)
                :mfunc(_mfunc),ctx(_ctx),interval(_mfunc,_ctx) { }
    void fillLinerScaner();
    void ScanLiveinterval();
    void expireOldIntervals(std::pair<Register*,rangeInfoptr> newInterval);
    void spillInterval(std::pair<Register*,rangeInfoptr> );
    void distributeRegs(std::pair<Register*,rangeInfoptr>);
    bool isFloatReg(Register* reg);
    int getAvailableRegNum(Register* reg);
    int allocateStackLocation();
    void initializeRegisterPool();
    void ReWriteRegs();
};

