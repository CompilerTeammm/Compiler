#pragma once

template<typename MyPass,typename MyType>
class _PassBase
{
public:
    virtual bool run() = 0;
    _PassBase() = default;
    virtual ~_PassBase() = default;
};


// 数据分析
template<typename MyAnalysis,typename MyType>
class _AnalysisBase
{
public:
    _AnalysisBase() = default;
    virtual ~_AnalysisBase() = default;
};