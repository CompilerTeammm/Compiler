#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "PassManager.hpp"
// 设计为一个抽象类

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