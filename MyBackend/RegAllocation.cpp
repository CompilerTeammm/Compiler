#include "../include/MyBackend/RegAllocation.hpp"

void RegAllocation::LinerScan()
{

}

void RegAllocation::ExpireOldIntervals()
{

}

void RegAllocation::SpillAtInterval()
{
    
}

bool RegAllocation::run()
{
    LinerScan();
    ExpireOldIntervals();
    SpillAtInterval();

    return true;
}