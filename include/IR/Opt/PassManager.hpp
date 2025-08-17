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
#include "SSAPRE.hpp"
#include "SimplifyCFG.hpp"
#include "CondMerge.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Analysis/AliasAnalysis.hpp"
#include "DSE.hpp"
#include "DAE.hpp"
#include "Global2Local.hpp"
#include "SelfStoreElimination.hpp"
#include "Inliner.hpp"
#include "TRE.hpp"
#include "SOGE.hpp"
#include "ConstantHoist.hpp"
#include "ECE.hpp"
#include "GepCombine.hpp"
#include "GepEval.hpp"
#include "GepFlatten.hpp"
#include "ExprReorder.hpp"

#include "../Analysis/LoopInfo.hpp"
#include "LoopSimplify.hpp"
#include "LoopRotate.hpp"
#include "LoopUnroll.hpp"
#include "LoopDel.hpp"
#include "Lcssa.hpp"
#include "Licm.hpp"
#include "SSR.hpp"

enum OptLevel
{
    None,
    O1,
    Test, // --test=GVN,DCE
    hu1_test
};

class PassManager
{
private:
    Module *module;
    std::unordered_set<std::string> enabledPasses;
    OptLevel level = None;

    AnalysisManager AM; // 只有这一个实例，Run里所有Pass共用
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
                //"SSE",
                // 前期规范化
                "mem2reg",
                //"sccp",
                //"SCFG",
                "ConstHoist",
                // "ECE",
                // 过程间优化
                //"inline",

                //"SOGE",
                // "G2L",
                // 局部清理
                //"DAE",
                //"TRE",
                // "CondMerge",

                // 循环优化
                "LoopSimplify",
                // "LoopRotate",
                //"LoopUnroll",
                //"LoopDel",
                //"Lcssa",
                //"Licm",
                //"SSR",

                // 数据流优化
                // "SSAPRE",
                //"GVN",
                //"DCE",
                //"ExprReorder",

                // 数组
                // "gepevalute",
                //"gepcombine",
                //"gepflatten",

                // 后端准备

            };
        }
        else if (lvl = hu1_test)
        {
            enabledPasses = {
                // 第一波
                "DAE",
                "SSE",
                "SOGE",
                "mem2reg",
                "G2L",
                "mem2reg",
                "sccp",
                "SCFG",
                "DCE",
                "SOGE",
                "sccp",
                "SCFG",

                // 第一次内联,循环清理两次
                "inline",
                "sccp",
                "SCFG",
                "DAE",
                "SOGE",
                "DCE",
                "TRE",
                "inline",
                "sccp",
                "SCFG",
                "DAE",
                "SOGE",
                "DCE",
                "TRE",
                // "ECE",
                // loop基础优化
                // "Loop_Simplifying",
                // loop展开+常规优化

                // 再来波常规清理
                "ExprReorder",
                "sccp",
                "SCFG",
                "SOGE",
                "DCE",
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

                for (auto &bb_ptr : func->GetBBs())
                {
                    BasicBlock *B = bb_ptr.get();
                    if (!B)
                        continue;
                    B->PredBlocks = tree.getPredBBs(B);
                    B->NextBlocks = tree.getSuccBBs(B);
                }
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
        if (IsEnabled("ConstantHoist"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();

                // 构建支配树分析，ConstantHoist 可能依赖它
                DominantTree tree(fun);
                tree.BuildDominantTree();
                AM.add<DominantTree>(fun, &tree);

                // 创建 ConstantHoist pass 并运行
                ConstantHoist CHPass(fun);
                CHPass.run();
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

        if (IsEnabled("G2L"))
        {
            Global2Local(&Singleton<Module>(), AM).run();
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
            SideEffect *se = new SideEffect(&Singleton<Module>());
            se->GetResult();
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();
                AM.add<DominantTree>(fun, &tree);
                AM.add<SideEffect>(&Singleton<Module>(), se);
                SelfStoreElimination(fun, AM).run();
                // 如果先跑SSE那就把这个打开
                //  for (auto& bb_ptr : fun->GetBBs()) {
                //      BasicBlock* B = bb_ptr.get();
                //      if (!B) continue;
                //      B->PredBlocks = tree.getPredBBs(B);
                //      B->NextBlocks = tree.getSuccBBs(B);
                //      }
            }
        }
        if (IsEnabled("CondMerge"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                CondMerge(fun, AM).run();
            }
        }
        if (IsEnabled("DAE"))
        {
            DAE(&Singleton<Module>(), AM).run();
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

       if (IsEnabled("LoopSimplify"))
        {
            for (auto &function : funcVec)
            {
                auto func = function.get();
                LoopSimplify(func, AM).run();
            }
        }
        if (IsEnabled("LCSSA"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 LcSSA(func, AM).run();
            }
        }

        if (IsEnabled("LICM"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 LICMPass(func, AM).run();
            }
        }

        if (IsEnabled("SSR"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 ScalarStrengthReduce(func, AM).run();
            }
        }

        if (IsEnabled("LoopRotate"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 LoopRotate(func, AM).run();
            }
        }

        if (IsEnabled("LoopUnroll"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 LoopUnroll(func, AM).run();
            }
        }

        if (IsEnabled("LoopDeletion"))
        {
            for (auto &function : funcVec)
            {
                 auto func = function.get();
                 LoopDeletion(func, AM).run();
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
                AM.add<DominantTree>(fun, &tree);
                SSAPRE(fun, AM).run();
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
                AM.add<DominantTree>(fun, &tree);
                GVN(fun, AM).run();
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

        if (IsEnabled("ExprReorder"))
        {
            for (auto &function : funcVec)
            {
                auto fun = function.get();
                DominantTree tree(fun);
                tree.BuildDominantTree();
                AM.add<DominantTree>(fun, &tree);
                ExprReorder(fun).run();
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

                AM.add<DominantTree>(fun, &tree);
                AM.add<SideEffect>(&Singleton<Module>(), se);

                GepCombine gepCombinePass(fun, AM);
                gepCombinePass.run();
            }
        }
        // 后端准备
    }
};