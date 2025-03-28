#pragma once
#include "Mem2reg.hpp"
#include "MemoryToRegister.hpp"
#include<queue>
#include"Passbase.hpp"
#include "CoreClass.hpp"
#include "CFG.hpp"
#include <memory>
#include"Singleton.hpp"

// #include "DCE.hpp"



enum PassName
{
    mem2reg_pass,
    inline_pass,
    dce_pass,
};

// PassManager 用来管理Passes
template<typename MyPass>
class PassManager :_PassBase<MyPass,Function>
{   
private:
    std::queue<PassName> Passque;
    Function* _func;
    Module* _mod;
public:
    void addPass(PassName pass) { Passque.emplace(pass); }
    PassName pushPass()
    {
        PassName pass = Passque.front();
        Passque.pop();
        return pass;
    }
    // 我这里从前端获取到内存形式的 M-SSA 
    PassManager() { _mod = &Singleton<Module>(); }
    void RunOnTest();
    
};

template<typename MyPass>
void PassManager<MyPass>:: RunOnTest()
{
    auto& funcVec = _mod->GetFuncTion();
    for(auto& function : funcVec)
    {
        auto fun = function.get();
        DominantTree tree(fun);
        Mem2reg(fun,&tree).run();
    }
}