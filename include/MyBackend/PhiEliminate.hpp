#pragma once
#include "BackendPass.hpp"
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"

class PhiEliminate : public BackendPassBase
{
    Function* func;
public:
    PhiEliminate(Function* _func)  :func(_func)  { }
    
    bool run() override;
};

