#pragma once
#include"Passbase.hpp"
#include "Mem2reg.hpp"
#include "MemoryToRegister.hpp"
#include<queue>
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include <memory>
#include"../../lib/Singleton.hpp"
#include"DCE.hpp"
#include"AnalysisManager.hpp"
// #include "DCE.hpp"
#define dce


enum PassName
{
    mem2reg_pass,
    inline_pass,
    dce_pass,
};

// PassManager 用来管理Passes
class PassManager :_PassBase<PassManager,Function>
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
    void run() override 
    {

    }
};


void PassManager:: RunOnTest()
{
    auto& funcVec = _mod->GetFuncTion();
    for(auto& function : funcVec)
    {
        // Function;
        auto fun = function.get();
        fun->GetSize() = 0;
        fun->GetBBs().clear();
        for(auto bb : *fun)
        {
            // BasicBlock
            bb->index = fun->GetSize()++;
            std::shared_ptr<BasicBlock> shared_bb(bb);
            fun->GetBBs().push_back(shared_bb);
        }
        DominantTree tree(fun);
        tree.BuildDominantTree();
        Mem2reg(fun,&tree).run();
    }
#ifdef dce
    for(auto& function : funcVec)
    {
        auto fun = function.get();
        AnalysisManager* AM;
        DCE(fun,AM).run();
    }
#endif
}