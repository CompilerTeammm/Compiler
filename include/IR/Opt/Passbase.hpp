#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "PassManager.hpp"


template<typename MyPass,typename MyType>
class _PassBase
{
public:
    virtual void run() = 0;
    _PassBase() = default;
    virtual ~_PassBase() = default;
};


// 数据
template<typename MyAnalysis,typename MyType>
class _AnalysisBase
{
public:
    _AnalysisBase() = default;
    virtual ~_AnalysisBase() = default;
};