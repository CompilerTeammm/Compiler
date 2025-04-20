#pragma once
#include "Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"

class AnalysisManager : public _AnalysisBase<AnalysisManager, Function>
{

public:
    void run();
    void get();
    AnalysisManager();
    ~AnalysisManager() = default;
};
