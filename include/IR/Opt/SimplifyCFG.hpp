#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"

#include <stack>
#include <unordered_set>
#include <optional>
#include <queue>
class SimplifyCFG : public _PassBase<SimplifyCFG, Function>
{
private:
    Function *func;

    //为了以防万一,要在每个优化前都进行一个_tree(func)的重建支配树,用到的vector不用回传tree,只需要新建即可
public:


    bool run() override;
    SimplifyCFG(Function *_func) : func(_func) {}
    ~SimplifyCFG() = default;
    // 分层次组织子优化
    bool SimplifyCFGFunction();
    // bool SimplifyCFGBasicBlock(BasicBlock *bb);

    // 子优化：function
    bool removeUnreachableBlocks(); // 删除不可达基本块(集成到simplifyBranch之后,方便)
    bool mergeEmptyReturnBlocks();  // 合并空返回基本块 仅处理操作数一致的空 ret 块，不考虑特殊控制流
    // 子优化：basicblock
    bool mergeBlocks(BasicBlock *bb);         // 合并基本块
    bool simplifyBranch();      // 简化分支（实际上是简化恒真或恒假的条件跳转
    bool CleanPhi(); // 消除无意义phi
    // bool mergeReturnJumps(BasicBlock *bb);
};