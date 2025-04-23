#pragma once
#include "Passbase.hpp"
#include "AnalysisManager.hpp"
#include <unordered_map>


// C++17 正式引入 [[nodiscard]] 通过编译时检查强制开发者处理关键返回值
// Global Value Numbering
// in the first this methond in one block but 
// now it is used to the whole program based on the SSA

class GVN:public _PassBase<GVN,Module>
{
public:
    GVN(Function*_func,DominantTree* _tree)
        :tree(_tree),func(_func) {}

    bool run() override;

private:
    // AnalysisManager* AM;
    Function* func;
    DominantTree* tree;
    std::unordered_map<Value*,int> ValTable;
};