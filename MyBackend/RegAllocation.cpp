#include "../include/MyBackend/RegAllocation.hpp"

#define INTREGNUM 15
#define FLOATREGNUM 20

//  int 可用 t0-t6     a0.a7 -->调用其他函数需要保存
//  float 可以  ft0-ft11   ， fa0-fa7 ---> 调用其他参数需要保存
// 如何分辨 int or float 呢？？？

bool RegAllocation::isFloatReg(Register* reg)
{
    return false;
}

int RegAllocation::getAvailableRegNum(Register* reg) {
    return isFloatReg(reg) ? FLOATREGNUM : INTREGNUM;
}

void RegAllocation::fillLinerScaner()
{
    interval.run();
    auto& intervals = interval.getIntervals();
    for(auto [reg,vec] : intervals) 
    {
        // inorder or unorder
        // int final_end = vec.front()->end;
        // int final_start = vec.back()->start;
        // LinerScaner.emplace_back(reg,std::make_shared <LiveInterval::range> (final_start,final_end));
        if (vec.empty()) 
            continue;

        int final_start = vec.front()->start;
        int final_end = vec.front()->end;

        for(auto& range : vec) {
            final_start = std::min(final_start,range->start);
            final_end = std::max(final_end,range->end);
        }
        LinerScaner.emplace_back(reg,std::make_shared <range> (final_start,final_end));
    }
    // 升序
    std::sort(LinerScaner.begin(), LinerScaner.end(),
              [](const auto &v1, const auto &v2)
              {
                  return v1.second->start < v1.second->start;
              });
}

void RegAllocation::expireOldIntervals(std::pair<Register*,LiveInterval::rangeInfoptr> newInterval)
{
    auto tmpList = active_list;  // security
    for(auto& oldInterval : tmpList)
    {
        if (oldInterval.second->end < newInterval.second->start) {
            Registerpool.emplace_back(oldInterval.first);
            active_list.remove(oldInterval);
            // why???
            if (oldInterval.first)
            {
                activeRegs.erase(oldInterval.first);
            }
        } else {
            break;
        }
    }
}

void RegAllocation::spillInterval(std::pair<Register*,rangeInfoptr> interval)
{
    if(active_list.empty()) {
        assert("needn't to spill");
    }

    auto spill = active_list.back();  // the last is the biggest;
    if(spill.second->end > interval.second->end) {

        activeRegs[interval.first] = activeRegs[spill.first];
        activeRegs.erase(spill.first);

        stackLocation[spill.first] = allocateStackLocation();

        active_list.remove(spill);
        active_list.emplace_back(interval);

        active_list.sort([](const auto &v1, const auto &v2){ 
                            return v1.second->end < v2.second->end; 
                         });
    }
    else {
        stackLocation[interval.first] = allocateStackLocation();
    }
}

void RegAllocation::distributeRegs(std::pair<Register*,rangeInfoptr> interval)
{   
    if(Registerpool.empty()) {
        spillInterval(interval);
        return;
    }
    auto reg = Registerpool.back();
    Registerpool.pop_back();

    activeRegs[interval.first] = reg;

    active_list.emplace_back(interval);

    active_list.sort([](const auto &v1, const auto &v2){ 
                        return v1.second->end < v2.second->end; 
                    });
}

void RegAllocation::initializeRegisterPool()
{
    // need to write a new func getRegister;
    Registerpool.clear();
        for (int i = Register::t0; i <= Register::t6; i++) {
        Registerpool.push_back(ctx->getRegister(i));
    }
    for (int i = Register::a0; i <= Register::a7; i++) {
        Registerpool.push_back(ctx->getRegister(i));
    }
    
    // 添加浮点可用寄存器
    for (int i = Register::ft0; i <= Register::ft11; i++) {
        Registerpool.push_back(ctx->getRegister(i));
    }
    for (int i = Register::fa0; i <= Register::fa7; i++) {
        Registerpool.push_back(ctx->getRegister(i));
    }
}

int RegAllocation::allocateStackLocation() 
{

}

void RegAllocation::ScanLiveinterval()
{
    initializeRegisterPool();

    for(auto& e : LinerScaner) 
    {   
        int regNum = getAvailableRegNum(e.first);
        // std::shared_ptr<LiveInterval::range> it = e.second;
        expireOldIntervals(e);

        if (active_list.size() == regNum) 
            spillInterval(e);
        else
        {
            distributeRegs(e);
        }
    }
}

bool RegAllocation::run()
{
    fillLinerScaner();
    ScanLiveinterval();

    return true;
}

