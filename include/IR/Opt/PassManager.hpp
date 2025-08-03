#pragma once
#include "GVN.hpp"
#include "Passbase.hpp"
#include "Mem2reg.hpp"
#include "MemoryToRegister.hpp"
#include <queue>
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include <memory>
#include "../../lib/Singleton.hpp"
#include "DCE.hpp"
#include "AnalysisManager.hpp"
#include "ConstantProp.hpp"
#include "GVN.hpp"
#include "LoopUnrolling.hpp"
#include "../../lib/Singleton.hpp"
#include "SSAPRE.hpp"
#include "SimplifyCFG.hpp"
#include "../Analysis/SideEffect.hpp"
#include "DSE.hpp"
#include "SelfStoreElimination.hpp"
#include "Inliner.hpp"
#include "TRE.hpp"
#include "SOGE.hpp" 
#include "ECE.hpp" 
#include "GepCombine.hpp" 
#include "GepEval.hpp" 

// 互不影响，完全没问题再放出来
// #define dce
// #define sccp
//#define pre
// #define SCFG
// #define DSE 用不到了重复了
// #define SSE
// 循环优化
// #define Loop_Simplifying
// #define Loop_Unrolling
//内联优化
// #define MY_INLINE_PASS
//TRE
// #define MY_TRE_PASS
//SOGE
// #define MY_SOGE_PASS
//ECE
// #define MY_ECE_PASS
//array
// #define gepcombine
// #define gepeval

enum PassName
{
    mem2reg_pass,
    // inline_pass,
    dce_pass,
};

// PassManager 用来管理Passes
class PassManager : _PassBase<PassManager, Function>
{
private:
    std::queue<PassName> Passque;
    Function *_func;
    Module *_mod;

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
    bool run() override
    {
        return true;
    }
};

inline void PassManager::RunOnTest()
{
    auto &funcVec = _mod->GetFuncTion();
    for (auto &function : funcVec)
    {
        // Function;
        auto fun = function.get();
        int bb_index = 0;
        fun->GetBBs().clear();
        for (auto bb : *fun)
        {
            // BasicBlock
            bb->index = bb_index++;
            std::shared_ptr<BasicBlock> shared_bb(bb);
            fun->GetBBs().push_back(shared_bb);
        }
        DominantTree tree(fun);
        tree.BuildDominantTree();
        Mem2reg(fun, &tree).run();
        for (auto& bb_ptr : fun->GetBBs()) {
        BasicBlock* B = bb_ptr.get();
        if (!B) continue;
        B->PredBlocks = tree.getPredBBs(B);
        B->NextBlocks = tree.getSuccBBs(B);
        }
    }

    //内敛优化
#ifdef dce
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        AnalysisManager *AM;
        DCE(fun, AM).run();
    }
#endif
#ifdef sccp
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        AnalysisManager *AM;
        ConstantProp(fun).run();
    }
#endif
#ifdef gvn
    for (auto &function : funcVec)
    {
        // Function;
        auto fun = function.get();
        DominantTree tree(fun);
        tree.BuildDominantTree();
        GVN(fun, &tree).run();
    }
#endif
#ifdef Loop_Simplying
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        AnalysisManager *AM;
        Loop_Simplying(fun, AM).run();
    }
#endif
#ifdef Loop_Unrolling
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        AnalysisManager *AM;
        LoopUnrolling(fun, AM).run();
    }
#endif
#ifdef pre
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        DominantTree tree(fun);
        tree.BuildDominantTree();
        SSAPRE(fun, &tree).run();
    }
#endif
#ifdef SCFG
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        SimplifyCFG(fun).run();
    }
#endif
#ifdef DSE

    SideEffect* se = new SideEffect(&Singleton<Module>());//在module级别做一次副作用分析
    se->GetResult();
    
    for (auto &function : funcVec)
    {   
        auto fun = function.get();
        DominantTree tree(fun);
        tree.BuildDominantTree();
        (DSE(fun, &tree,se)).run();
    }
#endif
#ifdef SSE
    for(auto &function : funcVec)
    {
        auto fun = function.get();
        DominantTree tree(fun);
        tree.BuildDominantTree();
        SelfStoreElimination(fun,&tree).run();
    }
#endif

#ifdef MY_INLINE_PASS
    Inliner inlinerPass(&Singleton<Module>());
    inlinerPass.run();
#endif
#ifdef MY_TRE_PASS
for (auto &function : funcVec)
{
    auto fun = function.get();
    TRE TREPass(fun);
    TREPass.run();
}
#endif
#ifdef MY_SOGE_PASS
    SOGE sogePass(&Singleton<Module>());
    sogePass.run();
#endif
#ifdef MY_ECE_PASS
for (auto &function : funcVec) {
    auto fun = function.get();
    ECE ECEpass(fun);
    ECEpass.run();
}
#endif
#ifdef gepcombine
SideEffect* se = new SideEffect(&Singleton<Module>());
se->GetResult();

for (auto &function : funcVec) {
    auto fun = function.get();
    DominantTree tree(fun);
    tree.BuildDominantTree();

    AnalysisManager AM;
    AM.add<DominantTree>(fun, &tree);
    AM.add<SideEffect>(&Singleton<Module>(), se);

    GepCombine gepCombinePass(fun, AM);
    gepCombinePass.run();
}
#endif

}     