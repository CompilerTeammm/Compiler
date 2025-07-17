#pragma once
#include "BackendPass.hpp"

// Regester Allocation (RA)
// I choose the LINEARSCANREGISTERALLOCATION 
class RegAllocation :public BackendPassBase
{
public:
    bool run() override; 

    void LinerScan();
    void ExpireOldIntervals();
    void SpillAtInterval();
};