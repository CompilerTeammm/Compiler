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
#include "DSE.hpp"
#include "Inliner.hpp"

// 互不影响，完全没问题再放出来
#define dce
#define sccp
// #define gvn
//#define pre
#define SCFG
// define DSE
// 循环优化
// #define Loop_Simplifying
// #define Loop_Unrolling
//内联优化
// #define inliner

enum PassName
{
    mem2reg_pass,
    inline_pass,
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

void PassManager::RunOnTest()
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
    }
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
        DominantTree tree(fun);
        tree.BuildDominantTree();
        SimplifyCFG(fun, &tree).run();
    }
#endif
#ifdef DSE
    for (auto &function : funcVec)
    {
        auto fun = function.get();
        DominantTree tree(fun);
        tree.BuildDominantTree();
        DSE(fun, &tree).run();
    }
#endif
#ifdef inliner
    // 直接在 module 上运行 inlinerPass
    Inliner inlinerPass(&Singleton<Module>());
    inlinerPass.run();
#endif
}