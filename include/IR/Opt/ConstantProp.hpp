#pragma once
#include"ConstantFold.hpp"
#include"Passbase.hpp"
#include "DCE.hpp"

class Function;

class ConstantProp:public _PassBase<ConstantProp,Function>
{
public:
    ConstantProp(Function* func)
                :_func(func) {}   
    void run() override;

private:
    ConstantFold FoldManager;
    Function* _func;
};