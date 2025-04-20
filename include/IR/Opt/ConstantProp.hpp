#pragma once
#include"ConstantFold.hpp"
#include"Passbase.hpp"
#include "DCE.hpp"

class Function;
class ConstantFold;

// 没有 bool 类型，识别不了
class ConstantProp:public _PassBase<ConstantProp,Function>
{
public:
    ConstantProp(Function* func)
                :_func(func) {}   
    bool run() override;

private:
    ConstantFold* FoldManager;
    Function* _func;
};