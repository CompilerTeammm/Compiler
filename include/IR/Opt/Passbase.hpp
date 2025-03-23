#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "PassManager.hpp"


template<typename MyPass,typename MyType>
class _PassBase
{
public:
    virtual void run() = 0;
};

// 数据
template<typename MyAnalysis,typename MyType>
class _AnalysisBase
{
public:
    
};