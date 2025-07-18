#include "MIR.hpp"

// LinerScaner use the LiveInterval
// LiveRange And LiveInterval is based on the mfunc
class LiveRange 
{
    std::vector<Register*> LiveUse;  // use
    std::vector<Register*> LiveDef;  // def 
public:
    RISCVFunction* curfunc;
    LiveRange(RISCVFunction* _curfunc)
            :curfunc(_curfunc), LiveUse{}, LiveDef{}  { }

    void GetLiveUseAndDef(); 
};

class LiveInterval:public LiveRange 
{
    using order = int;
    std::map<RISCVInst*,order> RecordInstAndOrder;
    void orderInsts();
public:
    void CalcuLiveIntervals();
};