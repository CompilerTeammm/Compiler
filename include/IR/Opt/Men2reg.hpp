#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "./Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "PassManager.hpp"

// 数据的分析如何与Men2reg进行对接，我不了解
// 实现对应的算法

// Mem2reg的实现需要大量的数据结构取供给
// 遍历的仅仅是一个 Function 也就是针对 BasicBlocks 的
class Mem2reg;
class Mem2reg : public _PassBase<Mem2reg ,Function>,public PromoteMem2Reg
{
using BBPtr = std::unique_ptr<BasicBlock>;
using InstPtr = std::unique_ptr<Instruction>;
public:
    Mem2reg(Function* function, DominantTree* tree) :_func(function), _tree(tree)
    {
        std::vector<BBPtr> BasicBlocks = _func->GetBBs();
        for(int i = 0; i < BasicBlocks.size(); i++)
        {
            // static_cast 派生类强转为基类
            // auto insts = BasicBlocks[i]->GetInsts();
            std::vector<InstPtr> insts = BasicBlocks[i] ->GetInsts();
            for(int i = 0; i <insts.size();i++)
            {
                auto& inst_ptr = insts[i];
                Instruction* raw_ptr = inst_ptr.get();
                AllocaInst* allca = dynamic_cast<AllocaInst*> (raw_ptr);
                if(allca)
                {
                    if(isAllocaPromotable(allca))
                    {
                        Allocas.push_back(allca);
                    }
                }
            }
        }
    }

    void run();
private:
    Function *_func;
    DominantTree * _tree;
    std::vector<AllocaInst *> Allocas;
};

void Mem2reg::run()
{
    
}


