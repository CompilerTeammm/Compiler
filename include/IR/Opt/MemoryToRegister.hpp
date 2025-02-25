#include "../../lib/CoreClass.hpp"
#include "Passbase.hpp"

class PromoteMem2Reg
{
public:
  // 遍历基本块中的指令，将指令进行一个消除 alloca/ store / load指令
    void promoteMemoryToRegister();
    bool isAllocaPromotable(AllocaInst *AI)
    {
      return true;
    }
    void reNameValue();
    bool promoteMemoryToRegister(Function *func);

    void run();

private:
    std::vector<AllocaInst*> Allocas;
};

// mem2reg pass 对应类是 PromoteLegacyPass  是一个FunctionPass  调用 函数
//  promoteMemoryToRegister  收集 Promotable 的 AllocaInst 然后调用函数 PromoteMemToReg
// AllocaInst 是 Promotable 的， AllocaInst 没有被用于 volatile instruction 并且它直接被用于 LoadInst 或 StoreInst（即没有被取过地址）
//   isAllocaPromotable   则认为该 AllocaInst 是 Promotable 
//  for 循环依次处理每一个 AllocaInst ，
