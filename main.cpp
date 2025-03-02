///// for test
#include <memory>
#include <bits/unique_ptr.h>
#include "./include/IR/Opt/PassManager.hpp"
#include "MemoryToRegister.hpp"

class Mem2Reg;
int main()
{
   auto func = Get();
   auto passManager = std::make_unique<PassManager>();
   passManager->addPass(mem2reg);
   passManager->RunImpl<Mem2Reg, Function>(func);
}