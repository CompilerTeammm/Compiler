#pragma once
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"

static bool promoteMemoryToRegister(DominantTree* tree,Function *func,std::vector<AllocaInst *>& Allocas)
{
    
}

void Mem2reg::run()
{
    if(Allocas.empty())
    {
        std::cout << "Allocas is empty" << std::endl;
        return;
    }
    // 通过构造临时对象去执行优化
    bool value =PromoteMem2Reg::promoteMemoryToRegister(_tree,_func,Allocas);
    if(!value)
        std::cout << "promoteMemoryToRegister failed "<<std::endl; 
}

bool PromoteMem2Reg::isAllocaPromotable(AllocaInst* AI)
{
    ValUseList& listPtr = AI->GetValUseList();
    for(Use* use: AI->GetUserUseList())
}