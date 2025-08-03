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
template<typename MyAnalysis, typename MyType>
class _AnalysisBase {
public:
    _AnalysisBase() = default;
    virtual ~_AnalysisBase() = default;
    virtual const MyAnalysis *GetResult(MyType *func) const { return nullptr; }

    MyAnalysis* GetDerived() {return static_cast<MyAnalysis*>(this);}
    const MyAnalysis* GetDerived() const {return static_cast<const MyAnalysis*>(this);}
};
