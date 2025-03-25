#pragma once
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "PassManager.hpp"


template<typename MyPass,typename MyType>
class _PassBase
{
public:
    void run() {
        static_cast<MyPass*>(this)->runImpl();  // CRTP核心模式
    } 

};

// 数据
template<typename MyAnalysis,typename MyType>
class _AnalysisBase
{
public:
    
};