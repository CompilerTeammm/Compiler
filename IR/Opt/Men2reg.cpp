#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"

void PromoteMem2Reg::run()
{
    
}

bool PromoteMem2Reg::promoteMemoryToRegister(Function* func)
{
    // 需要用到一个函数
    for(auto& e :Allocas)
    {
        isAllocaPromotable(e);
    }
}
