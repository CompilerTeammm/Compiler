///// for test
#include<memory>
#include <bits/unique_ptr.h>
#include "./include/IR/Opt/PassManager.hpp"




int main()
{
   auto passManager = std::make_unique<PassManager> ();
   passManager->RunOnFunction();
}