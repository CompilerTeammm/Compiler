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

class Global2Local : public _PassBase<Global2Local, Module>
{
    private:
    Module *module;              
    AnalysisManager &AM;         

    std::unordered_set<Var*> safeGlobals; // 可安全提升的全局变量集合
    std::unordered_map<Function*, std::unordered_set<Function*>> FuncSucc;
    std::unordered_map<Function*, int> CallNum; // 可复用到 createCallNum
    std::unordered_set<Function*> RecursiveFuncs;

    // 构建函数调用后继信息
    void createSuccFuncs();

    // 统计每个函数被调用次数
    void createCallNum();

    // 检测递归函数
    void detectRecursive();

    // 判断变量地址是否逃逸
    bool addressEscapes(Value *V);

    // 将全局变量 GV 提升到函数 F
    bool promoteGlobal(Var *GV, Function *F);

    bool isSimplePtrToSelf(Value *ptr, Value *V);
    bool usesValue(Value* val, Var* GV);

public:
    bool run()override;
    Global2Local(Module *m, AnalysisManager &AM) : module(m), AM(AM) {}
    ~Global2Local() {
        for (auto l : DeleteLoop)
          delete l;
        }
};
*/
