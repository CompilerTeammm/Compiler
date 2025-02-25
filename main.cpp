///// for test
#include<memory>
#include <bits/unique_ptr.h>
#include "./include/IR/Opt/PassManager.hpp"
#include "MemoryToRegister.hpp"


class Mem2Reg;
int main()
{
   Function* func;
   auto passManager = std::make_unique<PassManager> ();
   passManager->addPass(Mem2reg);
   passManager->RunImpl<PromoteMem2Reg>(func);
   
}