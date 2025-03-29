#pragma once
#include "Passbase.hpp"

class AnalysisManager:public _AnalysisBase<AnalysisManager,Function>
{
private:
    void run();
    Function* _func;
};




