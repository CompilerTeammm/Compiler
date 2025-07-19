#pragma once
#include "MIR.hpp"
#include "RISCVContext.hpp"

// LinerScaner use the LiveInterval
// LiveInfo And LiveInterval is based on the mfunc
class LiveInfo 
{
public:
    std::unordered_map<RISCVBlock*,std::set<Register*>> BlockLiveIn;
    std::unordered_map<RISCVBlock*,std::set<Register*>> BlockLiveOut;
    RISCVFunction* curfunc;
    std::shared_ptr<RISCVContext> ctx;
    LiveInfo(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext> _ctx)
            :curfunc(_curfunc),ctx(_ctx),BlockLiveIn{},BlockLiveOut{} { }

    void GetLiveUseAndDef(); 
    void CalcuLiveInAndOut();
};

class LiveInterval
{
    using order = int;
    std::unordered_map<RISCVInst*,order> RecordInstAndOrder;
    void orderInsts();
    LiveInfo liveInfo;
    RISCVFunction* curfunc;
    std::shared_ptr<RISCVContext> ctx;
    // bbInfos records the bb with its start and end
public:
    struct range
    {
        int start;
        int end;
        range(int s, int e) : start(s), end(e) {}
    };
    using rangeInfoptr = std::shared_ptr<range>;
private:
    std::unordered_map<RISCVBlock*,rangeInfoptr> bbInfos;

    std::unordered_map<Register*,std::vector<rangeInfoptr>> regLiveIntervals;
public:
    void CalcuLiveIntervals();
    LiveInterval(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext> _ctx)
                :liveInfo(_curfunc,_ctx),bbInfos{},curfunc(_curfunc),ctx(_ctx)  {  }

    void run();
    std::unordered_map<Register*,std::vector<rangeInfoptr>>& getIntervals() {
        return regLiveIntervals;
    }
};