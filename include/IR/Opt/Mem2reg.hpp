#pragma once
#include "./Passbase.hpp"
#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"

// 数据的分析如何与Men2reg进行对接，我不了解
// 实现对应的算法

// Mem2reg的实现需要大量的数据结构取供给
// 遍历的仅仅是一个 Function 也就是针对 BasicBlocks 的
// 把 PromoteMem2reg 设计为 Mem2reg的一个辅助函数

// MemoryToRegister 设计为 Mem2reg的工具类
class Mem2reg;

class Mem2reg:public _PassBase<Mem2reg, Function>
{
public:
    Mem2reg(Function *function, DominantTree *_tree);
    ~Mem2reg() = default;
    void run();

    // could be Promoteable?  M-> R alloca 指令
    bool isAllocaPromotable(AllocaInst *AI);

private:
    std::vector<AllocaInst *> allocas;
    Function* func;
    DominantTree* tree;
};
