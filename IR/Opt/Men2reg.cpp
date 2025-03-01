#pragma once
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"

void PromoteMem2Reg::RemoveFromAList(unsigned& AllocaNum)
{
    Allocas[AllocaNum] = Allocas.back();
    Allocas.pop_back();
    AllocaNum--;
}

void PromoteMem2Reg::rewriteSingleStoreAlloca(unsigned& AllocaNum)
{

}

bool PromoteMem2Reg::promoteMemoryToRegister(DominantTree* tree,Function *func,std::vector<AllocaInst *>& Allocas)
{
    // 移除没有users 的 alloca指令
    for(unsigned AllocaNum = 0; AllocaNum != Allocas.size(); ++AllocaNum){
        AllocaInst* AI = Allocas[AllocaNum];
        if(!AI->isUsed()){
            delete AI;
            RemoveFromAList(AllocaNum);
            // continue; // 可有可无
        }

        auto it = Allocas[AllocaNum]->GetValUseList();
    }

    return true;
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

// could be Promoteable?  M-> R alloca 指令
bool PromoteMem2Reg::isAllocaPromotable(AllocaInst* AI)
{
    // ValUseList& listPtr = AI->GetValUseList();
    // Use* use --->  ValUseList 
    for(Use* use : AI->GetValUseList())  // value -> use -> user
    {
        User* user = use->GetUser();
        if(LoadInst* LInst = dynamic_cast<LoadInst*> (user))
        {
            assert(LInst);
        }
        else if(StoreInst* SInst = dynamic_cast<StoreInst*> (user))
        {
            // 这种情况是将地址进行存储的情况  store 语句的特点是仅仅只有一个 Use啊
            if(SInst->GetOperand(0) == AI)   // user -> use -> value
                return false;
        }
        else if(GepInst* GInst = dynamic_cast<GepInst*> (user))
        {
            // 其实这个判断可有可无
            return false;
        }
        else  { return false; }
    }
    return true;
}

