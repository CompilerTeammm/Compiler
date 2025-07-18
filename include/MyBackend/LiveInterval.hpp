#pragma once
#include "MIR.hpp"

// LinerScaner use the LiveInterval
// LiveRange And LiveInterval is based on the mfunc
class LiveRange 
{
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveIn;
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveOut;
public:
    RISCVFunction* curfunc;
    LiveRange(RISCVFunction* _curfunc)
            :curfunc(_curfunc) { }

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