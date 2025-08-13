#include "../include/MyBackend/RegAllocation.hpp"

#define INTREGNUM 24
#define FLOATREGNUM 32

// #define INT_FLOAT_POOL

// int      t2   a0-a7 t3-t6  s1-s11       24
// t0 & t1 will not be regalloced, and it is real temp register
// float  ft0-ft11  fa0-fa7  fs0-fs11      32

bool RegAllocation::isFloatReg(Register *reg)
{
    return reg->IsFloatFlag();
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
        LinerScaner.emplace_back(reg,std::make_shared<range>(final_start,final_end));
    }
    // 升序
    std::sort(LinerScaner.begin(), LinerScaner.end(),
              [](const auto &v1, const auto &v2)
              {
                  return v1.second->start < v2.second->start;
              });
}

void RegAllocation::initializeRegisterPool()
{
    RegisterVec& vecs = RegisterVec::GetRegVecs();
    RegisterIntpool = vecs.GetintRegVec();
    RegisterFloatpool = vecs.GetfloatRegVec();

#ifdef INT_FLOAT_POOL
    for (auto&e :RegisterIntpool)
    {
        std::cout << "------- Int Registers--------" << std::endl;
        std::cout << e->getName() << std::endl;
    }

    for (auto&e :RegisterFloatpool)
    {
        std::cout << "------- float Registers--------" << std::endl;
        std::cout << e->getName() << std::endl;
    }
#endif
}

void RegAllocation::expireOldIntervals(std::pair<Register*,LiveInterval::rangeInfoptr> newInterval)
{
    auto tmpList = active_list;  // security
    for(auto& oldInterval : tmpList)
    {
        if (oldInterval.second->end < newInterval.second->start) {
            auto realReg = activeRegs[oldInterval.first];
            if (oldInterval.first->IsFloatFlag())
                RegisterFloatpool.emplace_back(realReg);
            else   
                RegisterIntpool.emplace_back(realReg);

            active_list.remove(oldInterval);
            
        } else {
            break;
        }
    }
}


// Todo 溢出未考虑到， LiveReg 需要进行处理
// activeRegs;     map<vir,real> --> 虚拟寄存器分配的实际寄存器
// 如果发生溢出的话，应该有变量被搞出来，需要记录变量在那条语句被溢出了
int RegAllocation::allocateStackLocation() 
{
    return 0;
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
    RealRegister* reg;

    auto func = [&](std::vector<RealRegister*>& pool){
        if (pool.empty()){
            spillInterval(interval);
            return;
        }

        reg = pool.back();
        pool.pop_back();
    };

    if (interval.first->IsFloatFlag()) {
        func(RegisterFloatpool);
    } else {
        func(RegisterIntpool);
    }

    activeRegs[interval.first] = reg;
 
    active_list.emplace_back(interval);

    active_list.sort([](const auto &v1, const auto &v2){ 
                        return v1.second->end < v2.second->end; 
                    });
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

// 溢出尚且未考虑到
void RegAllocation::ReWriteRegs()
{
    for(auto [virReg,realReg] : activeRegs)
    {
        virReg->as<VirRegister>()->reWriteRegWithReal(realReg);
    }
}

bool RegAllocation::run()
{
    fillLinerScaner();
    ScanLiveinterval();
    ReWriteRegs();

    return true;
}

