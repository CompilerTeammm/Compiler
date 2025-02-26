#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "./Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "PassManager.hpp"

// 数据的分析如何与Men2reg进行对接，我不了解
// 实现对应的算法

// Mem2reg的实现需要大量的数据结构取供给
// 遍历的仅仅是一个 Function 也就是针对 BasicBlocks 的
class Mem2reg;
class Mem2reg : public _PassBase<Mem2reg ,Function>,public PromoteMem2Reg
{
using BBPtr = std::unique_ptr<BasicBlock>;
public:
    Mem2reg(Function* function, DominantTree* tree) :_func(function), _tree(tree)
    {
        std::vector<BBPtr> BasicBlocks = _func->GetBBs();
        for(int i = 0; i < BasicBlocks.size(); i++)
        {
            auto instructions = BasicBlocks[i]->instructions;
            for(int i = 0; i <instructions.size();i++)
            {
                // 判断是不是 allocas
                if(instructions[i])
                {
                    Allocas.pop_back(static_cast<AllocaInst>(instructions[i]));
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
    for(auto AI : Allocas)
    {
        if(isAllocaPromotable(AI))
        {
            
        }
    }
}


