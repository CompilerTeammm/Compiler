#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "PassManager.hpp"
// 参考了学长们的代码，我认为他们的设计其实并不合理，这里我给出我自己理解的设计
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