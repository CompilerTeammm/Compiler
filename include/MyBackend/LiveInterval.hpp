#pragma once
#include "MIR.hpp"
#include "RISCVContext.hpp"

// LinerScaner use the LiveInterval
// LiveInfo And LiveInterval is based on the mfunc
class LiveInfo 
{
public:
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveIn;
    std::map<RISCVBlock*,std::set<Register*>> BlockLiveOut;
    RISCVFunction* curfunc;
    std::shared_ptr<RISCVContext> ctx;
    LiveInfo(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext> _ctx)
            :curfunc(_curfunc),ctx(_ctx),BlockLiveIn{},BlockLiveOut{} { }

    void GetLiveUseAndDef(); 
    void CalcuLiveInAndOut();
};

class LiveInterval:public LiveInfo 
{
    using order = int;
    std::map<RISCVInst*,order> RecordInstAndOrder;
    void orderInsts();

    // bbInfos records the bb with its start and end 
    using rangeInfo = struct range;
    using rangeInfoptr = std::shared_ptr<rangeInfo>;
    std::map<RISCVInst*,rangeInfoptr> bbInfos;
    struct range {
        int start;
        int end;
    };
public:
    void CalcuLiveIntervals();
    LiveInterval(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext> _ctx)
                :LiveInfo(_curfunc,_ctx),bbInfos{}  {  }

    void run();
};