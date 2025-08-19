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
    std::vector<std::string> enabledPasses;
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
                "SSE",
                // 前期规范化
                "mem2reg",
                "sccp",
                "SCFG",
                "ConstantHoist",
                // "ECE",
                // 过程间优化
                "inline",

                "SOGE",
                "G2L",
                // 局部清理
                "DAE",
                "TRE",
                "DCE",
                "ExprReorder",
                "sccp",
                "SCFG",

                //"GVN",
                "DCE",
                // 循环优化
                "LoopSimplify",
                "Lcssa",
                "LICM",
                "LoopRotate",
                //"LoopUnroll",
                "LoopDeletion",
                //"SSR",

                // 数据流优化
                // "SSAPRE",
                //"GVN",
                //"DCE",
                //"ExprReorder",

                // 数组
                // "GepEvaluate",
                //"gepcombine",
                //"gepflatten",

                // 后端准备

            };
        }
        else if (lvl == hu1_test)
        {
            enabledPasses = {
                // 前期规范化
                "SSE",
                "mem2reg",
                "G2L",

                // 基础数据流优化
                "sccp",
                "SCFG",
                "ConstantHoist",

                "ExprReorder",

                // 循环优化

                // gep ,inline
                "GepEvaluate",
                "gepcombine",
                "gepflatten",
                "inline",
                "SOGE",
                "DAE",
                "TRE",
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
        enabledPasses = tags;
    }

    void Run()
    {
        auto &funcVec = module->GetFuncTion();
        for (auto &tag : enabledPasses)
        {
            // 前期规范化
            if (tag == "mem2reg")
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

            if (tag == "sccp")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    AnalysisManager *AM;
                    ConstantProp(fun).run();
                }
            }
            if (tag == "ConstantHoist")
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
            if (tag == "SCFG")
            {
                for (auto &func : funcVec)
                    SimplifyCFG(func.get()).run();
            }

            // 过程间优化
            if (tag == "inline")
            {
                Inliner inlinerPass(&Singleton<Module>());
                inlinerPass.run();
            }
            if (tag == "SOGE")
            {
                SOGE sogePass(&Singleton<Module>());
                sogePass.run();
            }
            if (tag == "G2L")
            {
                Global2Local(&Singleton<Module>(), AM).run();
            }

            // 数据流整理
            if (tag == "ECE")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    ECE ECEpass(fun);
                    ECEpass.run();
                }
            }

            // 局部清理
            if (tag == "SSE")
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
            if (tag == "CondMerge")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    CondMerge(fun, AM).run();
                }
            }
            if (tag == "DAE")
            {
                DAE(&Singleton<Module>(), AM).run();
            }
            if (tag == "TRE")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    TRE TREPass(fun);
                    TREPass.run();
                }
            }

            if (tag == "LoopSimplify")
            {
                for (auto &function : funcVec)
                {
                    auto *func = function.get();

                    func->num = 0;
                    auto &oldVec = func->GetBBs();
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        keep.emplace(sp.get(), sp);
                    }
                    std::vector<Function::BBPtr> rebuilt;
                    for (auto *bb : *func)
                    {
                        bb->num = func->num++;
                        auto it = keep.find(bb);
                        if (it != keep.end())
                        {
                            rebuilt.push_back(std::move(it->second)); // 复用原来的 shared_ptr
                        }
                        else
                        {
                            rebuilt.emplace_back(bb); // 谨慎：只有确认没有别的 shared_ptr 管这个 bb 时才安全
                        }
                    }

                    oldVec.swap(rebuilt);

                    LoopSimplify(func, AM).run();
                }
            }
            if (tag == "LCSSA")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    func->num = 0;
                    auto &oldVec = func->GetBBs();
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        keep.emplace(sp.get(), sp);
                    }
                    std::vector<Function::BBPtr> rebuilt;
                    for (auto *bb : *func)
                    {
                        bb->num = func->num++;
                        auto it = keep.find(bb);
                        if (it != keep.end())
                        {
                            rebuilt.push_back(std::move(it->second)); // 复用原来的 shared_ptr
                        }
                        else
                        {
                            rebuilt.emplace_back(bb); // 谨慎：只有确认没有别的 shared_ptr 管这个 bb 时才安全
                        }
                    }

                    oldVec.swap(rebuilt);
                    LcSSA(func, AM).run();
                }
            }
            if (tag == "LICM")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    func->num = 0;
                    auto &oldVec = func->GetBBs();
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        keep.emplace(sp.get(), sp);
                    }
                    std::vector<Function::BBPtr> rebuilt;
                    for (auto *bb : *func)
                    {
                        bb->num = func->num++;
                        auto it = keep.find(bb);
                        if (it != keep.end())
                        {
                            rebuilt.push_back(std::move(it->second)); // 复用原来的 shared_ptr
                        }
                        else
                        {
                            rebuilt.emplace_back(bb); // 谨慎：只有确认没有别的 shared_ptr 管这个 bb 时才安全
                        }
                    }

                    oldVec.swap(rebuilt);
                    LICMPass(func, AM).run();
                }
            }
            if (tag == "SSR")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    func->num = 0;
                    auto &oldVec = func->GetBBs();
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        keep.emplace(sp.get(), sp);
                    }
                    std::vector<Function::BBPtr> rebuilt;
                    for (auto *bb : *func)
                    {
                        bb->num = func->num++;
                        auto it = keep.find(bb);
                        if (it != keep.end())
                        {
                            rebuilt.push_back(std::move(it->second)); // 复用原来的 shared_ptr
                        }
                        else
                        {
                            rebuilt.emplace_back(bb); // 谨慎：只有确认没有别的 shared_ptr 管这个 bb 时才安全
                        }
                    }

                    oldVec.swap(rebuilt);
                    ScalarStrengthReduce(func, AM).run();
                }
            }

            if (tag == "LoopRotate")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    func->num = 0;

                    auto &oldVec = func->GetBBs();

                    // 使用 map 保留原来的 shared_ptr，不移动裸指针
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                        keep.emplace(sp.get(), sp); // 存 shared_ptr

                    // rebuilt vector
                    std::vector<Function::BBPtr> rebuilt;
                    rebuilt.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        sp->num = func->num++;
                        rebuilt.push_back(sp);
                    }

                    oldVec.swap(rebuilt);

                    LoopRotate(func, AM).run();
                }
            }
            if (tag == "LoopUnroll")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    LoopUnroll(func, AM).run();
                }
            }
            if (tag == "LoopDeletion")
            {
                for (auto &function : funcVec)
                {
                    auto func = function.get();
                    func->num = 0;
                    auto &oldVec = func->GetBBs();
                    std::unordered_map<BasicBlock *, Function::BBPtr> keep;
                    keep.reserve(oldVec.size());
                    for (auto &sp : oldVec)
                    {
                        keep.emplace(sp.get(), sp);
                    }
                    std::vector<Function::BBPtr> rebuilt;
                    for (auto *bb : *func)
                    {
                        bb->num = func->num++;
                        auto it = keep.find(bb);
                        if (it != keep.end())
                        {
                            rebuilt.push_back(std::move(it->second)); // 复用原来的 shared_ptr
                        }
                        else
                        {
                            rebuilt.emplace_back(bb); // 谨慎：只有确认没有别的 shared_ptr 管这个 bb 时才安全
                        }
                    }

                    oldVec.swap(rebuilt);
                    LoopDeletion(func, AM).run();
                }
            }

            // 数据流优化
            if (tag == "SSAPRE")
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
            if (tag == "GVN")
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
            if (tag == "DCE")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    AnalysisManager *AM;
                    DCE(fun, AM).run();
                }
            }
            if (tag == "ExprReorder")
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
            if (tag == "gepcombine")
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
            if (tag == "GepEvaluate")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    DominantTree tree(fun);
                    tree.BuildDominantTree();

                    AM.add<DominantTree>(fun, &tree);
                    GepEvaluate(fun, AM).run();
                }
            }
            if (tag == "gepflatten")
            {
                for (auto &function : funcVec)
                {
                    auto fun = function.get();
                    GepFlatten(fun).run();
                }
            }
        }
    }
};