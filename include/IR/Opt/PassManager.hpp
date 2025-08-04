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
#include "LoopSimping.hpp"

enum OptLevel
{
    None,
    O1,
    Test, // --test=GVN,DCE
};

class PassManager
{
private:
    Module *module;
    std::unordered_set<std::string> enabledPasses;
    OptLevel level = None;

public:
    PassManager() : module(&Singleton<Module>()) {}

    void SetLevel(OptLevel lvl)
    {
        level = lvl;
        enabledPasses.clear();

        if (lvl == O1)
        {
            // 启用全部中端优化
            enabledPasses = {
                // 前期规范化
                "mem2reg",
                "sccp",
                "SCFG",

                // 过程间优化
                "inline",

                "SOGE",

                // 数据流整理
                // "ECE",

                // 局部清理
                // "SSE",
                "TRE",

                // 循环优化
                "Loop_Simplifying",
                // "Loop_Unrolling",

                // 数据流优化
                "SSAPRE",
                //"GVN",
                "DCE",

                // 数组重写
                // "gepcombine"

                // 后端准备

            };
        }
        else if (lvl == None)
        {
            enabledPasses = {}; // 默认都不开
        }
    }

    void EnableTestPasses(const std::vector<std::string> &tags)
    {
        level = Test;
        enabledPasses.clear();
        for (auto &tag : tags)
            enabledPasses.insert(tag);
    }

    bool IsEnabled(const std::string &tag) const
    {
        return enabledPasses.count(tag);
    }

    void Run()
    {
        auto &funcVec = module->GetFuncTion();
        // 前期规范化
        if (IsEnabled("mem2reg"))
        {
            for (auto &f : funcVec)
            {
                auto *func = f.get();
                int idx = 0;
                func->GetBBs().clear();
                for (auto *bb : *func)
                {
                    bb->index = idx++;
                    func->GetBBs().push_back(std::shared_ptr<BasicBlock>(bb));
                }
                DominantTree tree(func);
                tree.BuildDominantTree();
                Mem2reg(func, &tree).run();
            }
        }

        if (IsEnabled("sccp"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                AnalysisManager *AM;
                ConstantProp(fun).run();
            }
        }

        if (IsEnabled("SCFG"))
        {
            for (auto &func : funcVec)
                SimplifyCFG(func.get()).run();
        }

        // 过程间优化
        if (IsEnabled("inline"))
        {
            Inliner inlinerPass(&Singleton<Module>());
            inlinerPass.run();
        }

        if (IsEnabled("SOGE"))
        {
            SOGE sogePass(&Singleton<Module>());
            sogePass.run();
        }

        // 数据流整理
        if (IsEnabled("ECE"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                ECE ECEpass(fun);
                ECEpass.run();
            }
        }

        // 局部清理
        if (IsEnabled("SSE"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();
                SelfStoreElimination(fun, &tree).run();
            }
        }

        if (IsEnabled("TRE"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                TRE TREPass(fun);
                TREPass.run();
            }
        }

        // 循环优化
        if (IsEnabled("Loop_Simplifying"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                AnalysisManager *AM;
                LoopSimping(fun).run();
            }
        }

        if (IsEnabled("Loop_Unrolling"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                AnalysisManager *AM;
                LoopUnrolling(fun, AM).run();
            }
        }

        // 数据流优化
        if (IsEnabled("SSAPRE"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();
                SSAPRE(fun, &tree).run();
            }
        }

        if (IsEnabled("GVN"))
        {
            for (auto &function : funcVec)
            {
                // Function;
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();
                GVN(fun, &tree).run();
            }
        }

        if (IsEnabled("DCE"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                AnalysisManager *AM;
                DCE(fun, AM).run();
            }
        }

        // 数组重写
        if (IsEnabled("gepcombine"))
        {
            SideEffect *se = new SideEffect(&Singleton<Module>());
            se->GetResult();

            for (auto &function : funcVec)
            {
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();

                AnalysisManager AM;
                AM.add<DominantTree>(fun, &tree);
                AM.add<SideEffect>(&Singleton<Module>(), se);

                GepCombine gepCombinePass(fun, AM);
                gepCombinePass.run();
            }
        }
        // 后端准备
    }
};