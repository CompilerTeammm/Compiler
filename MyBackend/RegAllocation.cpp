#include "../include/MyBackend/RegAllocation.hpp"

void RegAllocation::fillLinerScaner()
{
    interval.run();
    auto& intervals = interval.getIntervals();
    for(auto [reg,vec] : intervals) 
    {
        int final_end = vec.front()->end;
        int final_start = vec.back()->start;
        LinerScaner.emplace_back(reg,std::make_shared <LiveInterval::range> (final_start,final_end));
    }
    std::sort(LinerScaner.begin(), LinerScaner.end(),
              [](const auto &v1, const auto &v2)
              {
                  return v1.second->start > v1.second->start;
              });
}

void RegAllocation::ScanLiveinterval()
{
    
}

bool RegAllocation::run()
{
    fillLinerScaner();
    return true;
}