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
    std::unordered_map<Register*,RISCVBlock*> DefValsInBlock;
    std::unordered_map<RealRegister*,RISCVInst*> RealRegAndInsts;
    
    RISCVFunction* curfunc;
    std::shared_ptr<RISCVContext> ctx;
    LiveInfo(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext>& _ctx)
            :curfunc(_curfunc),ctx(_ctx),BlockLiveIn{},BlockLiveOut{} { }

    void GetLiveUseAndDef(); 
    void CalcuLiveInAndOut();
    bool isDefValue(RISCVInst* mInst);
};

class LiveInterval
{
    using order = int;
    std::map<RISCVInst*,order> RecordInstAndOrder;
    std::map<order,RISCVInst*> RecordOrderAndInst;
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
    std::map<Register*,std::vector<rangeInfoptr>> regLiveIntervals;
    using position = int;
    std::map<RISCVInst*,position> CallAndPosRecord;
    // 实际寄存器 计算liveInterval 的时候必须把 realReg 寄存器也给计算上，否则是错误的
    std::unordered_map<RealRegister*,rangeInfoptr> realRegWithitRangeInfo; 
public:
    void CalcuLiveIntervals();
    LiveInterval(RISCVFunction* _curfunc,std::shared_ptr<RISCVContext>& _ctx)
                :liveInfo(_curfunc,_ctx),bbInfos{},curfunc(_curfunc),ctx(_ctx)  {  }

    std::map<RISCVInst*,position>& getCallAndPosRecord()  { return CallAndPosRecord; }
    void run();
    std::unordered_map<RealRegister*,rangeInfoptr>& getRealRegWithRange() { return realRegWithitRangeInfo; }
    std::map<Register*,std::vector<rangeInfoptr>>& getIntervals() {
        return regLiveIntervals;
    }
    void FinalCalcu();
    std::map<RISCVInst*,order>& getInstAndOrder() { return RecordInstAndOrder; }
    std::map<order,RISCVInst*>& getOrderAndInst() { return RecordOrderAndInst; }
};