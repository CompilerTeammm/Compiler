#pragma once
#include "BackendPass.hpp"
#include "LiveInterval.hpp"
#include "MIR.hpp"
#include <algorithm>

// Regester Allocation (RA)
// I choose the LINEARSCANREGISTERALLOCATION 
class RegAllocation :public BackendPassBase
{
    std::shared_ptr<RISCVContext> ctx;
    RISCVFunction* mfunc;
    LiveInterval interval;
    std::vector<std::pair<Register*,LiveInterval::rangeInfoptr>> LinerScaner;
    std::list<LiveInterval::rangeInfoptr> active_list;
    
public:
    bool run() override; 
    RegAllocation(RISCVFunction* _mfunc,std::shared_ptr<RISCVContext> _ctx)
                :mfunc(_mfunc),ctx(_ctx),interval(_mfunc,_ctx)  {  }
    void fillLinerScaner();
    void ScanLiveinterval();
};

