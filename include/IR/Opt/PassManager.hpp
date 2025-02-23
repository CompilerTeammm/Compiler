#pragma once
#include"Passbase.hpp"
#include "CoreClass.hpp"

// 用来管理Passes
class FunctionPassManager;
class MoudlePassManager;
enum PassName {
    mem2reg,

};

class _AnalysisManager:
    public _AnalysisManagerBase<_AnalysisManager,Function>
{
    void InitPass(_AnalysisManager pass)
    { }
};
