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

// could be Promoteable? 
bool PromoteMem2Reg::isAllocaPromotable(AllocaInst* AI)
{
    // ValUseList& listPtr = AI->GetValUseList();
    // Use* use --->  ValUseList 
    for(Use* use : AI->GetValUseList())
    {
        User* user = use->GetUser();
        // dynamic_cast 用于多态类型的 向下转型， 将基类指针转换为派生类指针
        // 如果user 实际指向的是 LoadInst 对象或其派生类对象，转换成功，返回有效指针
        if(LoadInst* LInst = dynamic_cast<LoadInst*>(user))
        {
            assert(LInst && "Linst is nullptr and the turning is failed");
        }
        else if(StoreInst *SInst = dynamic_cast<StoreInst*>(user))
        {
            if(SInst->GetUserUseList()[0]->GetValue() == AI)
            {
                return false;
            }
        }
        else if()
        {
            return false;
        }
    }
    return true;
}