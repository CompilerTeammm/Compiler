/*
#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "../../IR/Analysis/LoopInfo.hpp"
#include "AnalysisManager.hpp"
#include "../../include/lib/Trival.hpp"
#include <vector>

class Global2Local:public _PassBase<Global2Local, Module>{
protected:
    DominantTree *dom_info;
    LoopInfoAnalysis *loopAnalysis;
    int MaxNum = 5;
    size_t CurrSize = 0;
    size_t MaxSize = 3064320;
    std::map<Function *, std::set<Function *>> SuccFuncs;
    std::set<Function *> RecursiveFunctions;
    std::map<Var *, std::set<Function *>>
        Global2Funcs; // 哪些func 用了这个 globalvalue
    std::vector<User *> insert_insts;
    std::map<Function *, int> CallTimes;
    Module *module;
    AnalysisManager &AM;

private:
    void init(Module *module);
    void createSuccFuncs(Module *module);
    void DetectRecursive(Module *module);
    void CalGlobal2Funcs(Module *module);
    // void visit(Function *func, std::set<Function *> &visited);
    void RunPass(Module *module);
    void LocalGlobalVariable(Var *val, Function *func);
    void LocalArray(Var *arr, AllocaInst *alloca, BasicBlock *block);
    bool CanLocal(Var *val, Function *func);
    bool hasChanged(int index, Function *func);
    void CreateCallNum(Module *module);
    std::vector<Loop *> DeleteLoop;

public:
    bool run()override;
    Global2Local(Module *m, AnalysisManager &AM) : module(m), AM(AM) {}
    ~Global2Local() {
        for (auto l : DeleteLoop)
          delete l;
        }
};
*/