#pragma once
#include "BackendPass.hpp"

// Regester Allocation (RA)
class RegAllocation :public BackendPassBase
{
public:
    bool run() override; 
};