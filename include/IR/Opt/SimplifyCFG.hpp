#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"

#include <stack>
#include <unordered_set>
class SimplifyCFG:public _PassBase<SimplifyCFG, Function>{
private:
    Function* func;
    DominantTree* tree;
public:
    bool run() override;
    SimplifyCFG(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    ~SimplifyCFG() = default;
    //分层次组织子优化
    bool SimplifyCFGFunction(Function* func);
    bool SimplifyCFGBasicBlock(BasicBlock* bb);

    //子优化：function
    bool removeUnreachableBlocks(Function* func);//删除不可达基本块
    bool mergeEmptyReturnBlocks(Function* func);//合并空返回基本块 仅处理操作数一致的空 ret 块，不考虑特殊控制流
    //子优化：basicblock
    bool mergeBlocks(BasicBlock* bb);//合并基本块
    bool simplifyBranch(BasicBlock* bb);//简化分支（实际上是简化恒真或恒假的条件跳转
    bool eliminateTrivialPhi(BasicBlock* bb);//消除无意义phi
};