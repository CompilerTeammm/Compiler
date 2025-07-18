#pragma once
#include "MIR.hpp"
#include "RISCVContext.hpp"

// LinerScaner use the LiveInterval
// LiveRange And LiveInterval is based on the mfunc
class LiveRange 
{
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveIn;
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveOut;
public:
    RISCVFunction* curfunc;
    std::shared_ptr<RISCVContext> ctx;
    LiveRange(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext> _ctx)
            :curfunc(_curfunc),ctx(_ctx),BlockLiveIn{},BlockLiveOut{} { }

    void GetLiveUseAndDef(); 
    void CalcuLiveInAndOut();
};

class LiveInterval:public LiveRange 
{
    using order = int;
    std::map<RISCVInst*,order> RecordInstAndOrder;
    void orderInsts();
public:
    void CalcuLiveIntervals();
};