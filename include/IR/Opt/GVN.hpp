#pragma once
#include "Passbase.hpp"
#include "AnalysisManager.hpp"


// C++17 正式引入 [[nodiscard]] 通过编译时检查强制开发者处理关键返回值
// Global Value Numbering
// in the first this methond in one block but 
// now it is used to the whole program based on the SSA

class GVN:public _PassBase<GVN,Module>
{
    GVN(AnalysisManager* _AM,Function*_func)
        :AM(_AM),func(_func) {}

    bool run() override ;
private:
    AnalysisManager* AM;
    Function* func;
};