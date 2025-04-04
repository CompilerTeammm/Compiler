#pragma once
#include"ConstantFold.hpp"
#include"Passbase.hpp"

class Function;
class ConstantFold;

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