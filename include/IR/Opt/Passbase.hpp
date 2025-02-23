#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
// 这个类完全为一个抽象类，之后的PassManager继承这个
// = 0,是纯虚函数 Pure Virtual Function ，强制子类必须实现这个函数
// =defualt 是生成默认的构造函数
template<class Pass,class myType>
class _AnalysisManagerBase{
private:
    virtual void InitPass(Pass pass) = 0;
public:
    _AnalysisManagerBase() = default;
};

template<class Pass,class myType>
class _PassManagerBase{
private:
    virtual void InitPass(Pass pass) = 0;
public:
    virtual bool Run() = 0;
    _PassManagerBase() = default;
    // virtual ~_PassManagerBase() = default;
};