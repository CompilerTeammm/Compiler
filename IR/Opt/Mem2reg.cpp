#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "./Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "Mem2reg.hpp"

// 所有变量的alloca这一套都在入口的基本块处
// 原来的实现存在问题，修改成 entryBB

class Mem2reg;
// Mem2reg::Mem2reg(Function *function, DominantTree *_tree)
//     : func(function), tree(_tree)
// {
//     std::vector<Function::BBPtr> BasicBlocks = function->GetBBs();
//     for (int i = 0; i < BasicBlocks.size(); i++)
//     {
//         // static_cast 派生类强转为基类
//         // auto insts = BasicBlocks[i]->GetInsts();
//         auto insts = BasicBlocks[i]->GetInsts();
//         for (int i = 0; i < insts.size(); i++)
//         {
//             auto &inst_ptr = insts[i];
//             Instruction *raw_ptr = inst_ptr.get();
//             AllocaInst *allca = dynamic_cast<AllocaInst *>(raw_ptr);
//             if (allca)
//             {
//                 if (isAllocaPromotable(allca))
//                 {
//                     allocas.push_back(allca);
//                 }
//             }
//         }
//     }
// }

Mem2reg::Mem2reg(Function *function, DominantTree *_tree)
    : func(function), tree(_tree)
{
    allocas.clear();
    auto EntryBB = func->GetFront();

    for(Instruction* e : *EntryBB)
    {
        if(AllocaInst* AI = dynamic_cast<AllocaInst*> (e))
        {
            if(isAllocaPromotable(AI))
            {
                allocas.push_back(AI);
            }
        }
    }
}

bool Mem2reg::isAllocaPromotable(AllocaInst *AI)
{
    // Only allow direct and non-volatile loads and stores...  llvm 原话 但是 .sy语言恐怕是没有volatile关键字
    // ValUseList& listPtr = AI->GetValUseList();
    // Use* use --->  ValUseList
    for (Use *use : AI->GetValUseList()) // value -> use -> user
    {
        User *user = use->GetUser();
        if (LoadInst *LInst = dynamic_cast<LoadInst *>(user))
        {
            assert(LInst);
        }
        else if (StoreInst *SInst = dynamic_cast<StoreInst *>(user))
        {
            // 这种情况是将地址进行存储的情况  store 语句的特点是仅仅只有一个 Use
            if (SInst->GetOperand(0) == AI) // user -> use -> value
                return false;
        }
        else if (GepInst *GInst = dynamic_cast<GepInst *>(user))
        {
            // 可以与else 归结到一起
            // 其实这个判断可有可无
            return false;
        }
        else
        {
            return false;
        }
    }
    return true;
}

void Mem2reg::run()
{
    if (allocas.empty())
    {
        std::cout << "Allocas is empty" << std::endl;
        return;
    }
    // 通过构造临时对象去执行优化
    bool value = PromoteMem2Reg(func, tree, allocas).promoteMemoryToRegister();
    if (!value)
        std::cout << "promoteMemoryToRegister failed " << std::endl;
}